#ifndef SREJ_WINDOW_H
#define SREJ_WINDOW_H

#include <stdint.h>

#include "srej_packet.h"

typedef struct SenderWindowEntry
{
	uint32_t seq;
	int packet_len;
	int used;
	uint8_t packet[SREJ_MAX_PACKET];
} SenderWindowEntry;

typedef struct SenderWindow
{
	SenderWindowEntry *entry;
	int size;
	uint32_t bottom;
	uint32_t next;
} SenderWindow;

void sender_window_init(SenderWindow *window, int size, uint32_t start_seq);
void sender_window_free(SenderWindow *window);
int sender_window_open(SenderWindow *window);
int sender_window_empty(SenderWindow *window);
void sender_window_add(SenderWindow *window, uint32_t seq,
		uint8_t *packet, int packet_len);
SenderWindowEntry *sender_window_find(SenderWindow *window, uint32_t seq);
SenderWindowEntry *sender_window_lowest(SenderWindow *window);
void sender_window_rr(SenderWindow *window, uint32_t rr_seq);

#endif
