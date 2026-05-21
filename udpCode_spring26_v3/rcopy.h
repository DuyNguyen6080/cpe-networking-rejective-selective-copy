#include <netinet/in.h>
#include <arpa/inet.h>
#include "Window.h"

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
    int had_error;
    int retry_count;
    uint32_t control_seq;
    struct sockaddr_in6 server;
    Window window;
} RcopyInfo;

int process_file(char **argv);
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
void handle_reject(RcopyInfo *info, uint8_t *packet, int packet_len);
void resend_packet(RcopyInfo *info, WindowEntry *entry);
void send_final_done(RcopyInfo *info);
