#include "PDU.h"

int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *playload, int payload_len) {

    int header_len = 7; // 4 seq len, 2 checksum, 1 flag
    
    uint32_t net_sequenceNumber = htonl(sequenceNumber);
    void *seq_cpy = memcpy(pduBuffer, &net_sequenceNumber, sizeof(uint32_t));
    
    
    void *flag_cpy = memcpy(pduBuffer + 6, &flag, sizeof(uint8_t));
    void *payload_cpy = memcpy(pduBuffer + 7, playload, payload_len);
    // add checksum
    uint16_t checksum = in_cksum(pduBuffer, header_len + payload_len);
    void *cksum = memcpy(pduBuffer + sizeof(uint32_t), &checksum, sizeof(checksum));
    if (seq_cpy == NULL || flag_cpy == NULL || payload_cpy == NULL || cksum == NULL)
    {
        perror("createPDU memcpy error");
        return -1;
    }
    
    return payload_len + header_len;
}

void printPDU(uint8_t *buffer, int pduLength) {
    
    uint32_t seq_num = *(uint32_t*) buffer;
    int int_seq_num = ntohl(seq_num);
    int16_t checksum = *(uint16_t*) (buffer + 4);
    uint8_t flag = *(uint8_t*) (buffer + 6);

    printf("PDU HEADER:\n");
    printf("\tseq num: %d\n", int_seq_num);
    printf("\tchecksum: %d\n", checksum);
    printf("\tflag: %d\n", flag);
    return;
}
