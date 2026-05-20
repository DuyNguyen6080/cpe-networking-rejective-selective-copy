/* Client side for selective reject rcopy. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "pollLib.h"
#include "srej_packet.h"
#include "srej_window.h"

/*
 * These .c files are included because the assignment says do not change
 * the Makefile, but the window and packet code still need to be separate.
 */
#include "srej_packet.c"
#include "srej_window.c"

#define MAX_TRIES 10
#define FIRST_SEQ 1
#define POLL_NOW 0
#define POLL_ONE_SEC 1000

typedef enum State
{
	START_STATE,
	FILENAME,
	SEND_DATA,
	WAIT_ON_ACK,
	SEND_EOF,
	DONE
} STATE;

typedef struct RcopyInfo
{
	int socket_num;
	int input_file;
	int window_size;
	int buffer_size;
	int file_done;
	int retry_count;
	uint32_t next_seq;
	struct sockaddr_in6 server;
	SenderWindow window;
} RcopyInfo;

void process_file(char **argv);
STATE start_state(char **argv, RcopyInfo *info);
STATE filename_state(char **argv, RcopyInfo *info);
STATE send_data_state(RcopyInfo *info);
STATE wait_on_ack_state(RcopyInfo *info);
STATE send_eof_state(RcopyInfo *info);
void check_args(int argc, char **argv);
void check_number_args(char **argv);
int open_input_file(char *file_name);
int send_filename_packet(char *out_name, RcopyInfo *info);
int process_controls(RcopyInfo *info, int wait_time);
void handle_rr(RcopyInfo *info, uint8_t *packet, int packet_len);
void handle_srej(RcopyInfo *info, uint8_t *packet, int packet_len);
void resend_packet(RcopyInfo *info, SenderWindowEntry *entry);
void send_final_done(RcopyInfo *info);

/* This function starts rcopy after checking the command line. */
int main(int argc, char *argv[])
{
	check_args(argc, argv);

	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);

	process_file(argv);

	return 0;
}

/* This function runs the rcopy state machine. */
void process_file(char **argv)
{
	RcopyInfo info;
	STATE state = START_STATE;

	memset(&info, 0, sizeof(RcopyInfo));

	while (state != DONE)
	{
		switch (state)
		{
			case START_STATE:
				state = start_state(argv, &info);
				break;

			case FILENAME:
				state = filename_state(argv, &info);
				break;

			case SEND_DATA:
				state = send_data_state(&info);
				break;

			case WAIT_ON_ACK:
				state = wait_on_ack_state(&info);
				break;

			case SEND_EOF:
				state = send_eof_state(&info);
				break;

			case DONE:
				break;

			default:
				state = DONE;
				break;
		}
	}

	if (info.input_file > 0)
	{
		close(info.input_file);
	}

	if (info.socket_num > 0)
	{
		close(info.socket_num);
	}

	sender_window_free(&info.window);
}

/* This function opens the file, socket, poll set, and window. */
STATE start_state(char **argv, RcopyInfo *info)
{
	int port_number = atoi(argv[7]);

	info->window_size = atoi(argv[3]);
	info->buffer_size = atoi(argv[4]);
	info->input_file = open_input_file(argv[1]);
	info->socket_num = setupUdpClientToServer(&info->server,
			argv[6], port_number);
	info->server.sin6_port = htons(port_number);
	info->next_seq = FIRST_SEQ;

	setupPollSet();
	addToPollSet(info->socket_num);

	return FILENAME;
}

/* This function sends the output filename until the server answers. */
STATE filename_state(char **argv, RcopyInfo *info)
{
	if (send_filename_packet(argv[2], info) == 1)
	{
		sender_window_init(&info->window, info->window_size,
				info->next_seq);
		return SEND_DATA;
	}

	return DONE;
}

/* This function sends data while the sender window is open. */
STATE send_data_state(RcopyInfo *info)
{
	uint8_t data[SREJ_MAX_DATA];
	uint8_t packet[SREJ_MAX_PACKET];
	int data_len = 0;
	int packet_len = 0;

	while (sender_window_open(&info->window) == 1 &&
			info->file_done == 0)
	{
		data_len = read(info->input_file, data, info->buffer_size);

		if (data_len < 0)
		{
			perror("read");
			return DONE;
		}

		if (data_len == 0)
		{
			info->file_done = 1;
			break;
		}

		packet_len = srej_make_packet(packet, info->next_seq,
				FLAG_DATA, data, data_len);

		safeSendto(info->socket_num, packet, packet_len, 0,
				(struct sockaddr *) &info->server,
				sizeof(info->server));

		sender_window_add(&info->window, info->next_seq,
				packet, packet_len);

		info->next_seq++;
		info->retry_count = 0;
		process_controls(info, POLL_NOW);
	}

	if (info->file_done == 1 && sender_window_empty(&info->window) == 1)
	{
		return SEND_EOF;
	}

	return WAIT_ON_ACK;
}

/* This function waits when the window is closed or draining. */
STATE wait_on_ack_state(RcopyInfo *info)
{
	SenderWindowEntry *entry = NULL;
	int got_control = process_controls(info, POLL_ONE_SEC);

	if (got_control > 0)
	{
		info->retry_count = 0;
		return SEND_DATA;
	}

	entry = sender_window_lowest(&info->window);

	if (entry == NULL)
	{
		return SEND_DATA;
	}

	info->retry_count++;

	if (info->retry_count > MAX_TRIES)
	{
		return DONE;
	}

	resend_packet(info, entry);
	return WAIT_ON_ACK;
}

/* This function sends EOF, waits for EOF ACK, then sends final DONE. */
STATE send_eof_state(RcopyInfo *info)
{
	uint8_t packet[SREJ_MAX_PACKET];
	uint8_t in_packet[SREJ_MAX_PACKET];
	int packet_len = 0;
	int in_len = 0;
	int addr_len = sizeof(info->server);
	int tries = 0;

	packet_len = srej_make_packet(packet, info->next_seq,
			FLAG_EOF, NULL, 0);

	while (tries < MAX_TRIES)
	{
		safeSendto(info->socket_num, packet, packet_len, 0,
				(struct sockaddr *) &info->server,
				sizeof(info->server));

		if (pollCall(POLL_ONE_SEC) >= 0)
		{
			addr_len = sizeof(info->server);
			in_len = safeRecvfrom(info->socket_num, in_packet,
					SREJ_MAX_PACKET, 0,
					(struct sockaddr *) &info->server,
					&addr_len);

			if (srej_check_packet(in_packet, in_len) == 1 &&
					srej_get_flag(in_packet) == FLAG_EOF_ACK)
			{
				send_final_done(info);
				return DONE;
			}
		}

		tries++;
	}

	return DONE;
}

/* This function checks all command line arguments. */
void check_args(int argc, char **argv)
{
	if (argc != 8)
	{
		printf("usage: %s from-filename to-filename window-size "
				"buffer-size error-rate remote-machine remote-port\n",
				argv[0]);
		exit(-1);
	}

	if (strlen(argv[1]) > SREJ_MAX_NAME || strlen(argv[2]) > SREJ_MAX_NAME)
	{
		printf("Error: filename is too long\n");
		exit(-1);
	}

	check_number_args(argv);
}

/* This function checks the numeric command line arguments. */
void check_number_args(char **argv)
{
	int window_size = atoi(argv[3]);
	int buffer_size = atoi(argv[4]);
	double error_rate = atof(argv[5]);

	if (window_size <= 0 || window_size >= 1073741824)
	{
		printf("Error: bad window size\n");
		exit(-1);
	}

	if (buffer_size <= 0 || buffer_size > SREJ_MAX_DATA)
	{
		printf("Error: bad buffer size\n");
		exit(-1);
	}

	if (error_rate < 0 || error_rate > 1)
	{
		printf("Error rate must be >=0 and <=1\n");
		exit(-1);
	}
}

/* This function opens the input file or prints the required error. */
int open_input_file(char *file_name)
{
	int file_fd = open(file_name, O_RDONLY);

	if (file_fd < 0)
	{
		printf("Error: file %s not found\n", file_name);
		exit(-1);
	}

	return file_fd;
}

/* This function sends the filename setup packet and waits for response. */
int send_filename_packet(char *out_name, RcopyInfo *info)
{
	uint8_t packet[SREJ_MAX_PACKET];
	uint8_t payload[SREJ_MAX_NAME + 9];
	uint8_t in_packet[SREJ_MAX_PACKET];
	int packet_len = 0;
	int in_len = 0;
	int name_len = strlen(out_name) + 1;
	int addr_len = sizeof(info->server);
	int tries = 0;

	srej_write_u32(payload, info->window_size);
	srej_write_u32(payload + 4, info->buffer_size);
	memcpy(payload + 8, out_name, name_len);

	packet_len = srej_make_packet(packet, info->next_seq,
			FLAG_FILENAME, payload, name_len + 8);

	while (tries < MAX_TRIES)
	{
		safeSendto(info->socket_num, packet, packet_len, 0,
				(struct sockaddr *) &info->server,
				sizeof(info->server));

		if (pollCall(POLL_ONE_SEC) >= 0)
		{
			addr_len = sizeof(info->server);
			in_len = safeRecvfrom(info->socket_num, in_packet,
					SREJ_MAX_PACKET, 0,
					(struct sockaddr *) &info->server,
					&addr_len);

			if (srej_check_packet(in_packet, in_len) == 1 &&
					srej_get_flag(in_packet) == FLAG_FILENAME_RESP)
			{
				if (in_len < SREJ_HEADER_LEN + 1 ||
						in_packet[SREJ_HEADER_LEN] != 0)
				{
					printf("Error on open of output file: %s\n",
							out_name);
					return 0;
				}

				info->next_seq++;
				return 1;
			}
		}

		tries++;
	}

	return 0;
}

/* This function receives and handles all ready RR/SREJ packets. */
int process_controls(RcopyInfo *info, int wait_time)
{
	uint8_t packet[SREJ_MAX_PACKET];
	int packet_len = 0;
	int addr_len = sizeof(info->server);
	int count = 0;
	int ready_socket = pollCall(wait_time);

	while (ready_socket >= 0)
	{
		addr_len = sizeof(info->server);
		packet_len = safeRecvfrom(info->socket_num, packet,
				SREJ_MAX_PACKET, 0,
				(struct sockaddr *) &info->server,
				&addr_len);

		if (srej_check_packet(packet, packet_len) == 1)
		{
			if (srej_get_flag(packet) == FLAG_RR)
			{
				handle_rr(info, packet, packet_len);
				count++;
			}
			else if (srej_get_flag(packet) == FLAG_SREJ)
			{
				handle_srej(info, packet, packet_len);
				count++;
			}
		}

		ready_socket = pollCall(POLL_NOW);
	}

	return count;
}

/* This function handles one RR packet. */
void handle_rr(RcopyInfo *info, uint8_t *packet, int packet_len)
{
	uint32_t rr_seq = 0;

	if (packet_len < SREJ_HEADER_LEN + 4)
	{
		return;
	}

	rr_seq = srej_read_u32(packet + SREJ_HEADER_LEN);
	sender_window_rr(&info->window, rr_seq);
}

/* This function handles one SREJ packet. */
void handle_srej(RcopyInfo *info, uint8_t *packet, int packet_len)
{
	uint32_t srej_seq = 0;
	SenderWindowEntry *entry = NULL;

	if (packet_len < SREJ_HEADER_LEN + 4)
	{
		return;
	}

	srej_seq = srej_read_u32(packet + SREJ_HEADER_LEN);
	entry = sender_window_find(&info->window, srej_seq);

	if (entry != NULL)
	{
		resend_packet(info, entry);
	}
}

/* This function resends one packet from the sender window. */
void resend_packet(RcopyInfo *info, SenderWindowEntry *entry)
{
	safeSendto(info->socket_num, entry->packet, entry->packet_len, 0,
			(struct sockaddr *) &info->server,
			sizeof(info->server));
}

/* This function sends the last teardown packet from rcopy to server. */
void send_final_done(RcopyInfo *info)
{
	uint8_t packet[SREJ_MAX_PACKET];
	int packet_len = 0;

	packet_len = srej_make_packet(packet, info->next_seq + 1,
			FLAG_DONE, NULL, 0);

	safeSendto(info->socket_num, packet, packet_len, 0,
			(struct sockaddr *) &info->server,
			sizeof(info->server));
}
