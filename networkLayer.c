#include "network_layer.h"
#include <string.h>
#include <stdio.h>

#define STATIC_ROUTING_TABLE_SIZE 2  // Adjust size as needed

static RoutingTableEntry routing_table[STATIC_ROUTING_TABLE_SIZE] = {
    {{192, 168, 1, 2}, 0}, // Destination IP mapped to channel 0
    {{192, 168, 1, 3}, 1}  // Destination IP mapped to channel 1
};
static uint8_t local_ip[IP_ADDR_LEN] = {192, 168, 1, 1};  // Local IP address

// Helper function to compare two IP addresses
static int compare_ip(uint8_t* ip1, uint8_t* ip2) {
    return memcmp(ip1, ip2, IP_ADDR_LEN) == 0;
}

// Function to find the appropriate channel for a given destination IP
static int find_route(uint8_t* dest_ip) {
    for (int i = 0; i < STATIC_ROUTING_TABLE_SIZE; i++) {
        if (compare_ip(routing_table[i].dest_ip, dest_ip)) {
            return routing_table[i].channel;
        }
    }
    return -1; // Route not found
}

// Initialize the network layer
void network_layer_init() {
    user_msg_handler = receive_packet; 
}

// Send a packet to a specific destination IP address
void send_packet(uint8_t* dest_ip, uint8_t* data, uint8_t len) {
    if (len > MAX_PACKET_SIZE) {
        fprintf(stderr, "Data too large to send\n");
        return;
    }

    // Create the network packet
    NetworkPacket packet;
    memcpy(packet.src_ip, local_ip, IP_ADDR_LEN);
    memcpy(packet.dest_ip, dest_ip, IP_ADDR_LEN);
    memcpy(packet.data, data, len);
    packet.data_len = len;

    // Find the appropriate channel to send the packet
    int channel = find_route(dest_ip);
    if (channel == -1) {
        fprintf(stderr, "No route found to destination IP\n");
        return;
    }

    // Transmit the packet using the link layer
    manchester_transmit(channel, (uint8_t*)&packet, sizeof(NetworkPacket));
}

// Callback function to handle incoming messages from the link layer
void receive_packet(uint8_t* msg, int ch) {
    NetworkPacket* packet = (NetworkPacket*)msg;

    // Check if the packet is addressed to this device
    if (compare_ip(packet->dest_ip, local_ip)) {
        printf("Received packet from IP: %d.%d.%d.%d\n", 
            packet->src_ip[0], packet->src_ip[1], packet->src_ip[2], packet->src_ip[3]);
        printf("Data: %.*s\n", packet->data_len, packet->data);
    } else {
        printf("Packet not for this IP, ignoring.\n");
    }
}
