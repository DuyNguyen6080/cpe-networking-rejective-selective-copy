#include <arpa/inet.h>
#include <string.h>

#include "cpe464.h"
#include "Packet.h"

/* This function writes one unsigned int in network order. */
void write_u32(uint8_t *place, uint32_t value)
{
	uint32_t net_value = htonl(value);

	memcpy(place, &net_value, sizeof(uint32_t));
}

/* This function reads one unsigned int from network order. */
uint32_t read_u32(uint8_t *place)
{
	uint32_t net_value = 0;

	memcpy(&net_value, place, sizeof(uint32_t));
	return ntohl(net_value);
}

/* This function builds one packet with our 7 byte header. */
int make_packet(uint8_t *packet, uint32_t seq, uint8_t flag,
		uint8_t *data, int data_len)
{
	unsigned short checksum = 0;
	int packet_len = SREJ_HEADER_LEN + data_len;

	write_u32(packet, seq);
	packet[4] = 0;
	packet[5] = 0;
	packet[6] = flag;

	if (data_len > 0)
	{
		memcpy(packet + SREJ_HEADER_LEN, data, data_len);
	}

	checksum = in_cksum((unsigned short *) packet, packet_len);
	memcpy(packet + 4, &checksum, sizeof(unsigned short));

	return packet_len;
}

/* This function returns 1 when the checksum is correct. */
int check_packet(uint8_t *packet, int packet_len)
{
	if (packet_len < SREJ_HEADER_LEN)
	{
		return 0;
	}

	if (in_cksum((unsigned short *) packet, packet_len) == 0)
	{
		return 1;
	}

	return 0;
}

/* This function gets the packet sequence number from the header. */
uint32_t get_seq(uint8_t *packet)
{
	return read_u32(packet);
}

/* This function gets the flag byte from the header. */
uint8_t get_flag(uint8_t *packet)
{
	return packet[6];
}
