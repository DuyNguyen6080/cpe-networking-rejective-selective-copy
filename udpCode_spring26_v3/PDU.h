
#ifndef PDU_H
#define PDU_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include "cpe464.h"
int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *playload, int payload_len);
void printPDU(uint8_t *buffer, int pduLength);
#endif