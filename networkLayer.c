#include "network_layer.h"
#include <string.h>
#include <stdio.h>

//static routing table (pre-configured)
typedef struct {
    uint8_t dest_addr;  //1-byte destination address
    int channel;        //corresponding channel (0-3)
} RoutingTableEntry;

#define STATIC_ROUTING_TABLE_SIZE 2

static RoutingTableEntry routing_table[STATIC_ROUTING_TABLE_SIZE] = {
    {2, 0}, //computer with address 2 mapped to channel 0
    {3, 1}, //computer with address 3 mapped to channel 1
    {4, 2}, //computer with address 4 mapped to channel 2
    {5, 3}  //computer with address 5 mapped to channel 3 
};

//local device address (hardcoded)
static uint8_t local_address = 1;  //local device has address 1

//helper function to find the channel for a given destination address
static int find_route(uint8_t dest_addr) {
    for (int i = 0; i < STATIC_ROUTING_TABLE_SIZE; i++) {
        if (routing_table[i].dest_addr == dest_addr) {
            return routing_table[i].channel;
        }
    }
    return -1; //if route not found
}

//initialize the network layer
void network_layer_init() {
    //set the link layer's message callback to the network layer's receive handler
    set_msg_callback(receive_packet);
}

//send a packet to a specific destination address
void send_packet(uint8_t dest_addr, uint8_t* data, uint8_t len) {
    if (len > MAX_PACKET_SIZE) {
        printf("Data too large to send\n");
        return;
    }

    //create the network packet to send
    uint8_t packet[MAX_PACKET_SIZE + 3];  //src_addr, dest_addr, data_len, data
    packet[0] = local_address;            //add source address
    packet[1] = dest_addr;                //add destination address
    packet[2] = len;                      //add length byte
    memcpy(&packet[3], data, len);        //add data

    //find the right channel to send the packet
    int channel = find_route(dest_addr);
    if (channel == -1) {
        printf("No route found to destination address %d\n", dest_addr);
        return;
    }

    //transmit the packet using the link layer
    manchester_transmit(channel, packet, len + 3); //send the packet (address + length + data)
}

//callback function to handle incoming messages from the link layer
void receive_packet(uint8_t* msg, int ch) {
    uint8_t src_addr = msg[0];      //first byte is the source address
    uint8_t dest_addr = msg[1];     //second byte is the destination address
    uint8_t data_len = msg[2];      //third byte is the length of the data
    uint8_t* data = &msg[3];        //remaining bytes are the actual data

    //check if the packet is addressed to this device
    if (dest_addr == local_address) {
        printf("Received packet from address: %d on channel %d\n", src_addr, ch);
        printf("Data: %.*s\n", data_len, data);
    } else {
        printf("Packet not for this device, ignoring.\n");
    }
}
