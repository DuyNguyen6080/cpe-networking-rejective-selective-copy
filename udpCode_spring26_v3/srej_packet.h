#ifndef SREJ_PACKET_H
#define SREJ_PACKET_H

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

int srej_make_packet(uint8_t *packet, uint32_t seq, uint8_t flag,
        uint8_t *data, int data_len);
int srej_check_packet(uint8_t *packet, int packet_len);
uint32_t srej_get_seq(uint8_t *packet);
uint8_t srej_get_flag(uint8_t *packet);
void srej_write_u32(uint8_t *place, uint32_t value);
uint32_t srej_read_u32(uint8_t *place);

#endif
