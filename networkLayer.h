#ifndef NETWORK_LAYER_H
#define NETWORK_LAYER_H

#include <stdint.h>
#include "link_layer.h"

#define IP_ADDR_LEN 4
#define MAX_PACKET_SIZE 256
#define MAX_ROUTING_TABLE_ENTRIES 10

typedef struct {
   uint8_t src_ip[IP_ADDR_LEN];
   uint8_t dest_ip[IP_ADDR_LEN];
   uint8_t data[MAX_PACKET_SIZE];
   uint8_t data_len;
} NetworkPacket;

typedef struct {
   uint8_t dest_ip[IP_ADDR_LEN];
   int channel;
} RoutingTableEntry;

// Network layer initialization function
void network_layer_init();
void send_packet(uint8_t* dest_ip, uint8_t* data, uint8_t len);
void receive_packet(uint8_t* msg, int ch);

#endif





