/* Server side for selective reject rcopy. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"
#include "Packet.h"
#include "Buffer.h"

#include "Packet.c"
#include "Buffer.c"

#define MAX_TRIES 10
#define FIRST_DATA_SEQ 2
#define POLL_NOW 0
#define POLL_ONE_SEC 1000
#define POLL_TEN_SEC 10000

typedef enum State
{
	START,
	FILENAME,
	RECV_DATA,
	WAIT_ON_DONE,
	DONE
} STATE;

typedef struct ServerInfo
{
	int socket_num;
	int output_file;
	int window_size;
	int buffer_size;
	int retry_count;
	uint32_t control_seq;
	uint8_t first_packet[SREJ_MAX_PACKET];
	int first_packet_len;
	uint8_t last_ack[SREJ_MAX_PACKET];
	int last_ack_len;
	struct sockaddr_in6 client;
	Buffer buffer;
} ServerInfo;

int process_args(int argc, char **argv);
void process_server(int socket_num, double error_rate);
void process_client(int main_socket, uint8_t *packet, int packet_len,
		struct sockaddr_in6 *client, double error_rate);
STATE start_state(ServerInfo *info);
STATE filename_state(ServerInfo *info);
STATE recv_data_state(ServerInfo *info);
STATE wait_on_done_state(ServerInfo *info);
void handle_data_packet(ServerInfo *info, uint8_t *packet, int packet_len);
void handle_eof_packet(ServerInfo *info);
void send_filename_response(ServerInfo *info, uint8_t value);
void send_rr(ServerInfo *info);
void send_reject(ServerInfo *info, uint32_t seq);
void send_eof_ack(ServerInfo *info);
void resend_last_ack(ServerInfo *info);
void handle_zombies(int sig);

/* This function starts the server. */
int main(int argc, char *argv[])
{
	int socket_num = 0;
	int port_number = 0;
	double error_rate = 0;

	port_number = process_args(argc, argv);
	error_rate = atof(argv[1]);

	sendtoErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);

	socket_num = udpServerSetup(port_number);
	process_server(socket_num, error_rate);

	close(socket_num);
	return 0;
}

/* This function waits for clients and forks one child per file. */
void process_server(int socket_num, double error_rate)
{
	pid_t pid = 0;
	uint8_t packet[SREJ_MAX_PACKET];
	struct sockaddr_in6 client;
	int client_len = sizeof(client);
	int packet_len = 0;

	signal(SIGCHLD, handle_zombies);

	while (1)
	{
		client_len = sizeof(client);
		packet_len = safeRecvfrom(socket_num, packet, SREJ_MAX_PACKET,
				0, (struct sockaddr *) &client, &client_len);

		if (check_packet(packet, packet_len) == 1 &&
				get_flag(packet) == FLAG_FILENAME)
		{
			pid = fork();

			if (pid < 0)
			{
				perror("fork");
				exit(-1);
			}

			if (pid == 0)
			{
				process_client(socket_num, packet, packet_len,
						&client, error_rate);
				exit(0);
			}
		}
	}
}

/* This function runs the child server state machine. */
void process_client(int main_socket, uint8_t *packet, int packet_len,
		struct sockaddr_in6 *client, double error_rate)
{
	ServerInfo info;
	STATE state = START;

	memset(&info, 0, sizeof(ServerInfo));
	memcpy(info.first_packet, packet, packet_len);
	memcpy(&info.client, client, sizeof(struct sockaddr_in6));
	info.first_packet_len = packet_len;
	info.control_seq = 1;

	close(main_socket);
	sendtoErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);

	while (state != DONE)
	{
		switch (state)
		{
			case START:
				state = start_state(&info);
				break;

			case FILENAME:
				state = filename_state(&info);
				break;

			case RECV_DATA:
				state = recv_data_state(&info);
				break;

			case WAIT_ON_DONE:
				state = wait_on_done_state(&info);
				break;

			case DONE:
				break;

			default:
				state = DONE;
				break;
		}
	}

	if (info.output_file > 0)
	{
		close(info.output_file);
	}

	if (info.socket_num > 0)
	{
		close(info.socket_num);
	}

	buffer_free(&info.buffer);
}

/* This function creates the child socket and poll set. */
STATE start_state(ServerInfo *info)
{
	info->socket_num = createUdpSocket();

	setupPollSet();
	addToPollSet(info->socket_num);

	return FILENAME;
}

/* This function reads the filename packet and opens the output file. */
STATE filename_state(ServerInfo *info)
{
	uint8_t *payload = info->first_packet + SREJ_HEADER_LEN;
	char *file_name = (char *) payload + 8;

	info->window_size = read_u32(payload);
	info->buffer_size = read_u32(payload + 4);

	info->output_file = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (info->output_file < 0)
	{
		printf("server failed: output file could not be opened\n");
		printf("Reason: file path may be bad or permission is denied\n");
		send_filename_response(info, 1);
		return DONE;
	}

	send_filename_response(info, 0);
	buffer_init(&info->buffer, info->window_size, FIRST_DATA_SEQ,
			info->output_file);

	return RECV_DATA;
}

/* This function waits for data or EOF from rcopy. */
STATE recv_data_state(ServerInfo *info)
{
	uint8_t packet[SREJ_MAX_PACKET];
	int packet_len = 0;
	int client_len = sizeof(info->client);

	if (pollCall(POLL_TEN_SEC) < 0)
	{
		printf("server failed: data or EOF was not received\n");
		printf("Reason: rcopy may have stopped or packets may be lost\n");
		return DONE;
	}

	client_len = sizeof(info->client);
	packet_len = safeRecvfrom(info->socket_num, packet, SREJ_MAX_PACKET,
			0, (struct sockaddr *) &info->client, &client_len);

	if (check_packet(packet, packet_len) == 0)
	{
		return RECV_DATA;
	}

	if (get_flag(packet) == FLAG_DATA)
	{
		handle_data_packet(info, packet, packet_len);
		return RECV_DATA;
	}

	if (get_flag(packet) == FLAG_EOF)
	{
		handle_eof_packet(info);
		return WAIT_ON_DONE;
	}

	return RECV_DATA;
}

/* This function waits for the final DONE packet from rcopy. */
STATE wait_on_done_state(ServerInfo *info)
{
	uint8_t packet[SREJ_MAX_PACKET];
	int packet_len = 0;
	int client_len = sizeof(info->client);

	if (pollCall(POLL_ONE_SEC) < 0)
	{
		info->retry_count++;

		if (info->retry_count >= MAX_TRIES)
		{
			printf("server failed: final DONE was not received\n");
			printf("Reason: DONE packet may be lost or damaged\n");
			return DONE;
		}

		resend_last_ack(info);
		return WAIT_ON_DONE;
	}

	client_len = sizeof(info->client);
	packet_len = safeRecvfrom(info->socket_num, packet, SREJ_MAX_PACKET,
			0, (struct sockaddr *) &info->client, &client_len);

	if (check_packet(packet, packet_len) == 0)
	{
		return WAIT_ON_DONE;
	}

	if (get_flag(packet) == FLAG_DONE)
	{
		return DONE;
	}

	if (get_flag(packet) == FLAG_EOF)
	{
		resend_last_ack(info);
	}

	return WAIT_ON_DONE;
}

/* This function handles one good data packet. */
void handle_data_packet(ServerInfo *info, uint8_t *packet, int packet_len)
{
	uint32_t seq = get_seq(packet);
	int data_len = packet_len - SREJ_HEADER_LEN;
	uint8_t *data = packet + SREJ_HEADER_LEN;

	if (seq < buffer_expected(&info->buffer))
	{
		send_rr(info);
		return;
	}

	if (buffer_in_window(&info->buffer, seq) == 0)
	{
		send_rr(info);
		return;
	}

	buffer_save(&info->buffer, seq, data, data_len);

	if (seq == buffer_expected(&info->buffer))
	{
		buffer_clear_reject(&info->buffer, seq);
		buffer_write_ready(&info->buffer);
		send_rr(info);

		if (buffer_has_later(&info->buffer) == 1 &&
				buffer_need_reject(&info->buffer) == 1)
		{
			send_reject(info, buffer_expected(&info->buffer));
		}
	}
	else if (buffer_need_reject(&info->buffer) == 1)
	{
		send_reject(info, buffer_expected(&info->buffer));
	}
}

/* This function handles EOF after all data has arrived. */
void handle_eof_packet(ServerInfo *info)
{
	send_eof_ack(info);
	info->retry_count = 0;
}

/* This function sends the filename response packet. */
void send_filename_response(ServerInfo *info, uint8_t value)
{
	uint8_t packet[SREJ_MAX_PACKET];
	uint8_t payload[1];
	int packet_len = 0;

	payload[0] = value;
	packet_len = make_packet(packet, info->control_seq,
			FLAG_FILENAME_RESP, payload, 1);
	info->control_seq++;

	safeSendto(info->socket_num, packet, packet_len, 0,
			(struct sockaddr *) &info->client,
			sizeof(info->client));
}

/* This function sends an RR for the next expected data packet. */
void send_rr(ServerInfo *info)
{
	uint8_t packet[SREJ_MAX_PACKET];
	uint8_t payload[4];
	int packet_len = 0;

	write_u32(payload, buffer_expected(&info->buffer));
	packet_len = make_packet(packet, info->control_seq,
			FLAG_RR, payload, 4);
	info->control_seq++;

	safeSendto(info->socket_num, packet, packet_len, 0,
			(struct sockaddr *) &info->client,
			sizeof(info->client));
}

/* This function sends one reject packet for the missing packet. */
void send_reject(ServerInfo *info, uint32_t seq)
{
	uint8_t packet[SREJ_MAX_PACKET];
	uint8_t payload[4];
	int packet_len = 0;

	write_u32(payload, seq);
	packet_len = make_packet(packet, info->control_seq,
			FLAG_SREJ, payload, 4);
	info->control_seq++;

	safeSendto(info->socket_num, packet, packet_len, 0,
			(struct sockaddr *) &info->client,
			sizeof(info->client));
}

/* This function sends EOF ACK and saves it for possible resend. */
void send_eof_ack(ServerInfo *info)
{
	info->last_ack_len = make_packet(info->last_ack,
			info->control_seq, FLAG_EOF_ACK, NULL, 0);
	info->control_seq++;

	resend_last_ack(info);
}

/* This function resends the last EOF ACK packet. */
void resend_last_ack(ServerInfo *info)
{
	safeSendto(info->socket_num, info->last_ack, info->last_ack_len, 0,
			(struct sockaddr *) &info->client,
			sizeof(info->client));
}

/* This function checks the server command line. */
int process_args(int argc, char **argv)
{
	int port_number = 0;
	double error_rate = 0;

	if (argc < 2 || argc > 3)
	{
		printf("usage: %s error-rate [optional-port-number]\n", argv[0]);
		exit(-1);
	}

	error_rate = atof(argv[1]);

	if (error_rate < 0 || error_rate > 1)
	{
		printf("Error rate must be >=0 and <=1\n");
		exit(-1);
	}

	if (argc == 3)
	{
		port_number = atoi(argv[2]);
	}

	return port_number;
}

/* This function cleans up finished child processes. */
void handle_zombies(int sig)
{
	while (waitpid(-1, NULL, WNOHANG) > 0)
	{
	}

	signal(SIGCHLD, handle_zombies);
}
