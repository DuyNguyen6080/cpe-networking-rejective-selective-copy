#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#include "Packet.h"

typedef struct WindowEntry
{
	uint32_t seq;
	int packet_len;
	int used;
	uint8_t packet[SREJ_MAX_PACKET];
} WindowEntry;

typedef struct Window
{
	WindowEntry *entry;
	int size;
	uint32_t lower;
	uint32_t current;
	uint32_t upper;
} Window;

void window_init(Window *window, int size, uint32_t start_seq);
void window_free(Window *window);
int window_open(Window *window);
int window_empty(Window *window);
void window_add(Window *window, uint32_t seq,
		uint8_t *packet, int packet_len);
WindowEntry *window_find(Window *window, uint32_t seq);
WindowEntry *window_lowest(Window *window);
void window_rr(Window *window, uint32_t rr_seq);

#endif
