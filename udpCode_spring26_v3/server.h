#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Buffer.h"
#include "Packet.h"

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