#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Buffer.h"

/* This function makes the receiver circular Buffer. */
void buffer_init(Buffer *buffer, int size,
		uint32_t start_seq, int output_file)
{
	buffer->entry = (BufferEntry *) calloc(size,
			sizeof(BufferEntry));

	if (buffer->entry == NULL)
	{
		perror("calloc");
		exit(-1);
	}

	buffer->size = size;
	buffer->output_file = output_file;
	buffer->expected = start_seq;
	buffer->missing_seq = 0;
	buffer->missing_active = 0;
}

/* This function frees the receiver Buffer. */
void buffer_free(Buffer *buffer)
{
	if (buffer->entry != NULL)
	{
		free(buffer->entry);
	}
}

/* This function returns the next sequence number wanted. */
uint32_t buffer_expected(Buffer *buffer)
{
	return buffer->expected;
}

/* This function tells if a sequence number fits in the receive window. */
int buffer_in_window(Buffer *buffer, uint32_t seq)
{
	if (seq >= buffer->expected && seq < buffer->expected + buffer->size)
	{
		return 1;
	}

	return 0;
}

/* This function tells if the Buffer already has this packet. */
int buffer_has(Buffer *buffer, uint32_t seq)
{
	int spot = seq % buffer->size;

	if (buffer->entry[spot].used == 1 &&
			buffer->entry[spot].seq == seq)
	{
		return 1;
	}

	return 0;
}

/* This function tells if the Buffer has packets after the expected one. */
int buffer_has_later(Buffer *buffer)
{
	int count = 0;

	while (count < buffer->size)
	{
		if (buffer->entry[count].used == 1 &&
				buffer->entry[count].seq > buffer->expected)
		{
			return 1;
		}

		count++;
	}

	return 0;
}

/* This function stores an out of order data packet. */
void buffer_save(Buffer *buffer, uint32_t seq,
		uint8_t *data, int data_len)
{
	int spot = seq % buffer->size;

	if (buffer_has(buffer, seq) == 1)
	{
		return;
	}

	buffer->entry[spot].seq = seq;
	buffer->entry[spot].data_len = data_len;
	buffer->entry[spot].used = 1;
	memcpy(buffer->entry[spot].data, data, data_len);
}

/* This function writes every ready packet in order. */
void buffer_write_ready(Buffer *buffer)
{
	while (buffer_has(buffer, buffer->expected) == 1)
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

/* This function tells if the missing packet should be requested. */
int buffer_need_reject(Buffer *buffer)
{
	if (buffer->missing_active == 0 ||
			buffer->missing_seq != buffer->expected)
	{
		buffer->missing_active = 1;
		buffer->missing_seq = buffer->expected;
		return 1;
	}

	return 0;
}

/* This function clears the missing packet request after it arrives. */
void buffer_clear_reject(Buffer *buffer, uint32_t seq)
{
	if (buffer->missing_active == 1 && buffer->missing_seq == seq)
	{
		buffer->missing_active = 0;
	}
}
