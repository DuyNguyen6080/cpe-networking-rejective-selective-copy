#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

#include "Packet.h"

typedef struct BufferEntry
{
	uint32_t seq;
	int data_len;
	int used;
	uint8_t data[SREJ_MAX_DATA];
} BufferEntry;

typedef struct Buffer
{
	BufferEntry *entry;
	int size;
	int output_file;
	uint32_t expected;
	uint32_t missing_seq;
	int missing_active;
} Buffer;

void buffer_init(Buffer *buffer, int size,
		uint32_t start_seq, int output_file);
void buffer_free(Buffer *buffer);
uint32_t buffer_expected(Buffer *buffer);
int buffer_in_window(Buffer *buffer, uint32_t seq);
int buffer_has(Buffer *buffer, uint32_t seq);
int buffer_has_later(Buffer *buffer);
void buffer_save(Buffer *buffer, uint32_t seq,
		uint8_t *data, int data_len);
void buffer_write_ready(Buffer *buffer);
int buffer_need_reject(Buffer *buffer);
void buffer_clear_reject(Buffer *buffer, uint32_t seq);

#endif
