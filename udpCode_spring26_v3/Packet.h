#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

#define SREJ_HEADER_LEN 7
#define SREJ_MAX_DATA 1400
#define SREJ_MAX_PACKET 1407
#define SREJ_MAX_NAME 100

#define FLAG_DATA 3
#define FLAG_RR 5
#define FLAG_SREJ 6
#define FLAG_FILENAME 7
#define FLAG_FILENAME_RESP 8
#define FLAG_EOF 32
#define FLAG_EOF_ACK 33
#define FLAG_DONE 34

int make_packet(uint8_t *packet, uint32_t seq, uint8_t flag,
        uint8_t *data, int data_len);
int check_packet(uint8_t *packet, int packet_len);
uint32_t get_seq(uint8_t *packet);
uint8_t get_flag(uint8_t *packet);
void write_u32(uint8_t *place, uint32_t value);
uint32_t read_u32(uint8_t *place);

#endif
