#ifndef SREJ_BUFFER_H
#define SREJ_BUFFER_H

#include <stdint.h>

#include "srej_packet.h"

typedef struct ReceiverBufferEntry
{
	uint32_t seq;
	int data_len;
	int used;
	uint8_t data[SREJ_MAX_DATA];
} ReceiverBufferEntry;

typedef struct ReceiverBuffer
{
	ReceiverBufferEntry *entry;
	int size;
	int output_file;
	uint32_t expected;
	uint32_t srej_seq;
	int srej_active;
} ReceiverBuffer;

void receiver_buffer_init(ReceiverBuffer *buffer, int size,
		uint32_t start_seq, int output_file);
void receiver_buffer_free(ReceiverBuffer *buffer);
uint32_t receiver_buffer_expected(ReceiverBuffer *buffer);
int receiver_buffer_in_window(ReceiverBuffer *buffer, uint32_t seq);
int receiver_buffer_has(ReceiverBuffer *buffer, uint32_t seq);
void receiver_buffer_save(ReceiverBuffer *buffer, uint32_t seq,
		uint8_t *data, int data_len);
void receiver_buffer_write_ready(ReceiverBuffer *buffer);
int receiver_buffer_need_srej(ReceiverBuffer *buffer);
void receiver_buffer_clear_srej(ReceiverBuffer *buffer, uint32_t seq);

#endif
