#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include <stdint.h>

//constants
#define BUFFER_SIZE 128
#define BIT_DURATION_US 5000

//function pointer for message callback
typedef void (*msg_callback_t)(uint8_t* msg, int ch);

/**
 * @brief initialize link layer, set up GPIOs and callbacks
 * @return 0 on success, non-zero if failed
 */
int initialize_link_layer();

/**
 * @brief reset the state of the specified channel
 * @param ch index of the channel to reset (0-3) 
 */
void reset_channel_state(int ch);

/**
 * @brief set the callback function to handle received messages
 * @param callback the function pointer for the callback
 */
void set_msg_callback(msg_callback_t callback);

/**
 * @brief transmit a message using Manchester encoding on a specific channel
 * @param ch the index of the channel (0-3) or -1 to broadcast to all channels 
 * @param data pointer to the data to be transmitted
 * @param len the length of the data to be transmitted
 */
void manchester_transmit(int ch, uint8_t *data, uint8_t len);

/**
 * @brief compute the checksum for the given data 
 * @param data pointer to the data
 * @param len the length of the data array
 * @return the computed checksum.
 */
uint8_t compute_checksum(uint8_t *data, uint8_t len);

/**
 * @brief print the received message for debugging purposes
 * @param msg the message data
 * @param ch the channel index from which the message was received
 */
void print_callback(uint8_t* msg, int ch);

#endif // LINK_LAYER_H
