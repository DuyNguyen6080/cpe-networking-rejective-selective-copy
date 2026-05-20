#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srej_window.h"

/* This function makes the sender window array. */
void sender_window_init(SenderWindow *window, int size, uint32_t start_seq)
{
	window->entry = (SenderWindowEntry *) calloc(size,
			sizeof(SenderWindowEntry));

	if (window->entry == NULL)
	{
		perror("calloc");
		exit(-1);
	}

	window->size = size;
	window->bottom = start_seq;
	window->next = start_seq;
}

/* This function frees the sender window array. */
void sender_window_free(SenderWindow *window)
{
	if (window->entry != NULL)
	{
		free(window->entry);
	}
}

/* This function tells if another data packet can be sent. */
int sender_window_open(SenderWindow *window)
{
	if (window->next < window->bottom + window->size)
	{
		return 1;
	}

	return 0;
}

/* This function tells if all packets in the window are acked. */
int sender_window_empty(SenderWindow *window)
{
	if (window->bottom == window->next)
	{
		return 1;
	}

	return 0;
}

/* This function saves a sent packet in the circular window. */
void sender_window_add(SenderWindow *window, uint32_t seq,
		uint8_t *packet, int packet_len)
{
	int spot = seq % window->size;

	window->entry[spot].seq = seq;
	window->entry[spot].packet_len = packet_len;
	window->entry[spot].used = 1;
	memcpy(window->entry[spot].packet, packet, packet_len);

	if (seq == window->next)
	{
		window->next++;
	}
}

/* This function finds a packet that is still inside the window. */
SenderWindowEntry *sender_window_find(SenderWindow *window, uint32_t seq)
{
	int spot = seq % window->size;

	if (window->entry[spot].used == 1 &&
			window->entry[spot].seq == seq)
	{
		return &(window->entry[spot]);
	}

	return NULL;
}

/* This function returns the lowest unacked packet in the window. */
SenderWindowEntry *sender_window_lowest(SenderWindow *window)
{
	uint32_t seq = window->bottom;

	while (seq < window->next)
	{
		SenderWindowEntry *entry = sender_window_find(window, seq);

		if (entry != NULL)
		{
			return entry;
		}

		seq++;
	}

	return NULL;
}

/* This function removes all packets covered by an RR. */
void sender_window_rr(SenderWindow *window, uint32_t rr_seq)
{
	uint32_t seq = window->bottom;

	while (seq < rr_seq && seq < window->next)
	{
		int spot = seq % window->size;

		if (window->entry[spot].seq == seq)
		{
			window->entry[spot].used = 0;
		}

		seq++;
	}

	while (window->bottom < window->next &&
			sender_window_find(window, window->bottom) == NULL)
	{
		window->bottom++;
	}
}
