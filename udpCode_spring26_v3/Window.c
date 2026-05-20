#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Window.h"

/* This function makes the sender Window array. */
void window_init(Window *window, int size, uint32_t start_seq)
{
	window->entry = (WindowEntry *) calloc(size,
			sizeof(WindowEntry));

	if (window->entry == NULL)
	{
		perror("calloc");
		exit(-1);
	}

	window->size = size;
	window->lower = start_seq;
	window->current = start_seq;
	window->upper = start_seq + size;
}

/* This function frees the sender Window array. */
void window_free(Window *window)
{
	if (window->entry != NULL)
	{
		free(window->entry);
	}
}

/* This function tells if another data packet can be sent. */
int window_open(Window *window)
{
	if (window->current < window->upper)
	{
		return 1;
	}

	return 0;
}

/* This function tells if all packets in the Window are acked. */
int window_empty(Window *window)
{
	if (window->lower == window->current)
	{
		return 1;
	}

	return 0;
}

/* This function saves a sent packet in the circular Window. */
void window_add(Window *window, uint32_t seq,
		uint8_t *packet, int packet_len)
{
	int spot = seq % window->size;

	window->entry[spot].seq = seq;
	window->entry[spot].packet_len = packet_len;
	window->entry[spot].used = 1;
	memcpy(window->entry[spot].packet, packet, packet_len);

	if (seq == window->current)
	{
		window->current++;
	}
}

/* This function finds a packet that is still inside the Window. */
WindowEntry *window_find(Window *window, uint32_t seq)
{
	int spot = seq % window->size;

	if (window->entry[spot].used == 1 &&
			window->entry[spot].seq == seq)
	{
		return &(window->entry[spot]);
	}

	return NULL;
}

/* This function returns the lowest unacked packet in the Window. */
WindowEntry *window_lowest(Window *window)
{
	uint32_t seq = window->lower;

	while (seq < window->current)
	{
		WindowEntry *entry = window_find(window, seq);

		if (entry != NULL)
		{
			return entry;
		}

		seq++;
	}

	return NULL;
}

/* This function removes all packets covered by an RR. */
void window_rr(Window *window, uint32_t rr_seq)
{
	uint32_t seq = window->lower;

	while (seq < rr_seq && seq < window->current)
	{
		int spot = seq % window->size;

		if (window->entry[spot].seq == seq)
		{
			window->entry[spot].used = 0;
		}

		seq++;
	}

	while (window->lower < window->current &&
			window_find(window, window->lower) == NULL)
	{
		window->lower++;
	}

	window->upper = window->lower + window->size;
}
