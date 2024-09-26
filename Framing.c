#include <pigpiod_if2.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define HDLC_FLAG 0x7E          //start and end flag/header and trailer 
#define BIT_DELAY 0.0001        //we need to adjust the timing --- we need to fix this timing/clock/delay issue
#define MAX_DATA_SIZE 1500      //set a payload size max -- don't know how necessary this might be in the future 

//the struct for the HDLC Frame Structure -- we chose this particular structure/method for our frames 
struct hdlc_frame {
  uint8_t address;            //address field (1 byte)
  uint8_t control;            //control field (1 byte)
  uint8_t data[MAX_DATA_SIZE]; //data payload
  uint16_t fcs;               //Frame Check Sequence 
  size_t data_length;         //length of the data payload
};

//function to compute CRC-16 -- this needs work, this is a very beginner way that isn't working right now for us 
uint16_t compute_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
      crc ^= data[i];         //XOR current byte into the CRC
      for (int j = 0; j < 8; j++) {
          if (crc & 1) {      //if the last bit is 1
              crc = (crc >> 1) ^ 0x8408; //shift right and apply polynomial
          } else {
              crc >>= 1;      //otherwise just shift right
          }
      }
  }
  return ~crc; //invert the bits at the end
}

//we want to receive and transmit bytes 
void transmit_and_receive_byte(int pi, int tx_pin, int rx_pin, uint8_t tx_byte, uint8_t *rx_byte) {
  *rx_byte = 0;  //clear the received byte
  for (int bit = 0; bit < 8; bit++) {
      int bit_value = (tx_byte >> bit) & 1;   //extract the bit to send
      gpio_write(pi, tx_pin, bit_value);      //write it to the TX pin
      time_sleep(BIT_DELAY / 2);  //there has to be a bit delay to let the receiver process it

      int received_bit = gpio_read(pi, rx_pin); //read the bit from RX pin
      *rx_byte |= (received_bit << bit);        //reconstruct the byte one bit at a time

      time_sleep(BIT_DELAY / 2);  //another bit delay to synchronize for the next bit
      printf("Tx bit: %d, Rx bit: %d\n", bit_value, received_bit); //log the transmitted and received bits
  }
  printf(" (Transmitted byte: 0x%02X, Received byte: 0x%02X)\n", tx_byte, *rx_byte); //log the full byte transmission
}

//transmit and receive the frame, byte by byte
void transmit_and_receive_frame(int pi, int tx_pin, int rx_pin, struct hdlc_frame *frame) {
  uint8_t received_byte; //where we'll store received bytes

  //transmit and receive start flag (0x7E)
  transmit_and_receive_byte(pi, tx_pin, rx_pin, HDLC_FLAG, &received_byte);

  //transmit and receive address (1 byte)
  transmit_and_receive_byte(pi, tx_pin, rx_pin, frame->address, &received_byte);

  //transmit and receive control (1 byte)
  transmit_and_receive_byte(pi, tx_pin, rx_pin, frame->control, &received_byte);

  //transmit and receive data, byte by byte
  for (size_t i = 0; i < frame->data_length; i++) {
      transmit_and_receive_byte(pi, tx_pin, rx_pin, frame->data[i], &received_byte);
  }

  //compute and transmit CRC (2 bytes)
  uint16_t crc = compute_crc16((uint8_t *)frame, frame->data_length + 2); 
  transmit_and_receive_byte(pi, tx_pin, rx_pin, (crc & 0xFF), &received_byte);
  transmit_and_receive_byte(pi, tx_pin, rx_pin, ((crc >> 8) & 0xFF), &received_byte); 

  //transmit and receive stop flag (0x7E)
  transmit_and_receive_byte(pi, tx_pin, rx_pin, HDLC_FLAG, &received_byte);
}

int main() {
  //GPIO setup for port 1 
  int P1t = 27;  //TX pin
  int P1r = 26;  //RX pin

  int pi = pigpio_start(NULL, NULL);  //start the pigpio library
  set_mode(pi, P1t, PI_OUTPUT);       //set the TX pin to output
  set_mode(pi, P1r, PI_INPUT);        //set the RX pin to input

  //example frame to test that we are sending and receiving bits 
  struct hdlc_frame frame;
  frame.address = 0x01; //example address (temporary)
  frame.control = 0x03; //example control field (temporary)
  frame.data_length = 5;  //payload length -- this might cause issues in the future but we will reevaluate
  frame.data[0] = 'H';    //example data payload: "HELLO" 
  frame.data[1] = 'E';
  frame.data[2] = 'L';
  frame.data[3] = 'L';
  frame.data[4] = 'O';

  //transmit and receive frame synchronously
  transmit_and_receive_frame(pi, P1t, P1r, &frame);

  //stop the GPIO
  pigpio_stop(pi);

  return 0;
}
