
#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include <stdint.h>
#include <pigpiod_if2.h>

#define BUFFER_SIZE 128
#define BIT_DURATION_US 5000

extern int rx_pins[];
extern int tx_pins[];

extern int gpio_handle;

typedef struct {
   uint8_t half_bit_signal;
   uint8_t sync_detected;
   uint32_t prev_tick;
   uint32_t margin;
   uint8_t bit_buffer[8];
   int bit_pos;
   uint8_t msg_buffer[BUFFER_SIZE];
   int msg_pos;
} ChannelState;

extern ChannelState port_state[4];
extern uint8_t received_msg[BUFFER_SIZE];

//Callback function type definition for message handling
typedef void (*msg_callback_t)(uint8_t* msg, int ch);

//Function prototypes for the link layer

//Function to determine which port corresponds to a GPIO pin
int gpio_to_port(unsigned gpio_pin);

//Function to reset the port state
void reset_channel(ChannelState *ch_state);

//Function to convert bits to full byte
void bit_to_char(int ch_index);

//Handle received signal edges
void rx_callback(int pi, unsigned gpio, unsigned level, uint32_t tick);

//Function to transmit a message using Manchester encoding
void manchester_transmit(int port, uint8_t *msg, int len);

//Print callback to handle received message
void print_callback(uint8_t* msg, int port);

#endif
