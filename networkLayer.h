#ifndef NETWORK_LAYER_H
#define NETWORK_LAYER_H

#include <stdint.h>
#include "link_layer.h"

#define MAX_ADDRESS 255          //max value for 1-byte addresses
#define MAX_PACKET_SIZE 255      //max packet size for data
#define MAX_ROUTING_TABLE_ENTRIES 10 //max routing table entries

//network layer functions
/**
 * @brief initialize network layer
 */
void network_layer_init();

/**
 * @brief initialize link layer, set up GPIOs and callbacks
 * @param dest_addr the address we want to send to
 * @param data pointer to the data being transmitted 
 * @param len the length of the data being transmitted 
 */
void send_packet(uint8_t dest_addr, uint8_t* data, uint8_t len);

/**
 * @brief callback function to handle incoming messages from the link layer
 * @param msg pointer to the message recieved 
 * @param ch the channel the message has been received on 
 */
void receive_packet(uint8_t* msg, int ch);

#endif // NETWORK_LAYER_H





