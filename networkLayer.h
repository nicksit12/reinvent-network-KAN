// network_layer.h


#ifndef NETWORK_LAYER_H
#define NETWORK_LAYER_H


#include <stdint.h>
#include "linkLayer.h"  //Include the link layer header


#define IP_ADDR_LEN 4
#define MAX_PACKET_SIZE 256
#define MAX_ROUTING_TABLE_ENTRIES 10


//Packet structure for the network layer
typedef struct {
   uint8_t src_ip[IP_ADDR_LEN];    // Source IP address
   uint8_t dest_ip[IP_ADDR_LEN];   // Destination IP address
   uint8_t data[MAX_PACKET_SIZE];  // Data payload
   uint8_t data_len;               // Length of the data
} NetworkPacket;


typedef struct {
   uint8_t dest_ip[IP_ADDR_LEN];   
   int channel;                    
} RoutingTableEntry;


void network_layer_init(RoutingTableEntry* routing_table, int table_size, uint8_t* local_ip);
void send_packet(uint8_t* dest_ip, uint8_t* data, uint8_t len);
void receive_packet(uint8_t* msg, int ch); 


#endif 





