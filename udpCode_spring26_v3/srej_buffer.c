#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "srej_buffer.h"

/* This function makes the receiver circular buffer. */
void receiver_buffer_init(ReceiverBuffer *buffer, int size,
		uint32_t start_seq, int output_file)
{
	buffer->entry = (ReceiverBufferEntry *) calloc(size,
			sizeof(ReceiverBufferEntry));

	if (buffer->entry == NULL)
	{
		perror("calloc");
		exit(-1);
	}

	buffer->size = size;
	buffer->output_file = output_file;
	buffer->expected = start_seq;
	buffer->srej_seq = 0;
	buffer->srej_active = 0;
}

/* This function frees the receiver buffer. */
void receiver_buffer_free(ReceiverBuffer *buffer)
{
	if (buffer->entry != NULL)
	{
		free(buffer->entry);
	}
}

/* This function returns the next sequence number wanted. */
uint32_t receiver_buffer_expected(ReceiverBuffer *buffer)
{
	return buffer->expected;
}

/* This function tells if a sequence number fits in the receive window. */
int receiver_buffer_in_window(ReceiverBuffer *buffer, uint32_t seq)
{
	if (seq >= buffer->expected && seq < buffer->expected + buffer->size)
	{
		return 1;
	}

	return 0;
}

/* This function tells if the buffer already has this packet. */
int receiver_buffer_has(ReceiverBuffer *buffer, uint32_t seq)
{
	int spot = seq % buffer->size;

	if (buffer->entry[spot].used == 1 &&
			buffer->entry[spot].seq == seq)
	{
		return 1;
	}

	return 0;
}

/* This function stores an out of order data packet. */
void receiver_buffer_save(ReceiverBuffer *buffer, uint32_t seq,
		uint8_t *data, int data_len)
{
	int spot = seq % buffer->size;

	if (receiver_buffer_has(buffer, seq) == 1)
	{
		return;
	}

	buffer->entry[spot].seq = seq;
	buffer->entry[spot].data_len = data_len;
	buffer->entry[spot].used = 1;
	memcpy(buffer->entry[spot].data, data, data_len);
}

/* This function writes every ready packet in order. */
void receiver_buffer_write_ready(ReceiverBuffer *buffer)
{
	while (receiver_buffer_has(buffer, buffer->expected) == 1)
	{
		int spot = buffer->expected % buffer->size;
		int wrote = write(buffer->output_file, buffer->entry[spot].data,
				buffer->entry[spot].data_len);

		if (wrote < 0)
		{
			perror("write");
			exit(-1);
		}

		buffer->entry[spot].used = 0;
		buffer->expected++;
	}
}

/* This function tells if a new SREJ should be sent. */
int receiver_buffer_need_srej(ReceiverBuffer *buffer)
{
	if (buffer->srej_active == 0 || buffer->srej_seq != buffer->expected)
	{
		buffer->srej_active = 1;
		buffer->srej_seq = buffer->expected;
		return 1;
	}

	return 0;
}

/* This function clears the current SREJ when that packet arrives. */
void receiver_buffer_clear_srej(ReceiverBuffer *buffer, uint32_t seq)
{
	if (buffer->srej_active == 1 && buffer->srej_seq == seq)
	{
		buffer->srej_active = 0;
	}
}
