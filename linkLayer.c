#include <pigpiod_if2.h>  
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define TX_PIN 27             //  pin number for transmitting data
#define RX_PIN 26             //  pin number for receiving data
#define BUFFER_SIZE 128       //  size of the message buffer (more/less)
#define SYNC_BYTE 0xA5        //  byte used to synchronize transmitter and receiver
#define BIT_DURATION_US 1000  //  duration of each bit in microseconds (1ms

// Global variables for reception (Volatile for the sake of hardware )
volatile uint32_t last_tick = 0;          // Timestamp of the last edge detected
volatile int bit_index = 0;               // keep track of bits received
volatile uint8_t received_byte = 0;       // The byte currently being received
volatile int sync_detected = 0;           // flag to indicate if SYNC_BYTE has been detected
volatile uint8_t message_length = 0;      // length of the incoming message
volatile uint8_t received_message[BUFFER_SIZE];  // Buffer to store the received message
volatile int message_index = 0;           // keep track of received message bytes

// Function to transmit a byte using Manchester encoding
void manchester_transmit(int handle, int tx_pin, uint8_t byte) {
    gpioPulse_t pulses[16];  //each bit requires two pulses for Manchester encoding
    int pulse_count = 0;

    // Loop over each bit in the byte
    for (int i = 7; i >= 0; i--) {
        int bit = (byte >> i) & 1;  // get the current bit

        // Manchester encoding rules:
        // For bit '1', output High then Low
        // For bit '0', output Low then High
        // tx_pin used for clarity 
        pulses[pulse_count].gpioOn = bit ? (1 << tx_pin) : 0;        
        pulses[pulse_count].gpioOff = bit ? 0 : (1 << tx_pin);       
        pulses[pulse_count].usDelay = BIT_DURATION_US / 2;           
        pulse_count++;

        pulses[pulse_count].gpioOn = bit ? 0 : (1 << tx_pin);        // invert the signal for the second half of the bit
        pulses[pulse_count].gpioOff = bit ? (1 << tx_pin) : 0;
        pulses[pulse_count].usDelay = BIT_DURATION_US / 2;
        pulse_count++;
    }

    wave_clear(handle);                           // Clear any existing waveforms
    wave_add_generic(handle, pulse_count, pulses);  // add  pulses to the waveform
    int wave_id = wave_create(handle);          
    if (wave_id >= 0) {
        wave_send_once(handle, wave_id);          // Send the waveform once
        while (wave_tx_busy(handle)) {
            time_sleep(0.001);                    // time_sleep vs gpio library 
        }
        wave_delete(handle, wave_id);             // Delete the waveform to free resources
    } else {
        fprintf(stderr, "Could not create waveform\n");  // Error handling if waveform creation fails
    }
}

// Callback function that's called whenever an edge is detected on RX_PIN
void rx_callback(int pi, unsigned user_gpio, unsigned level, uint32_t tick) {
    static uint32_t last_tick_local = 0;  // Local variable to store the last tick
    static int last_level = PI_TIMEOUT;   // The last GPIO level observed

    if (level == PI_TIMEOUT) {
        printf(" timeout occured")
\        return;
    }

    if (last_tick_local == 0) {
        //first edge detected, initialize last_tick_local and last_level
        last_tick_local = tick;
        last_level = level;
        return;
    }

    uint32_t time_diff = tick - last_tick_local;  // Time difference between edges
    last_tick_local = tick;                       // Update last_tick_local

    // Check if the time difference is approximately half of BIT_DURATION_US
    if (time_diff > BIT_DURATION_US / 2 - 100 && time_diff < BIT_DURATION_US / 2 + 100) {
        // We're within the expected time frame for a bit transition
        received_byte <<= 1;  // Shift the received_byte to make room for the new bit
        if (level != last_level) {
            //If the level has changed, it's a 1
            received_byte |= 1;
        } else {
            //If the level hasn't changed, it's a 0
            received_byte |= 0;
        }
        bit_index++;  // Increment the bit index

        if (bit_index == 8) {
            // We've received 8 bits, which makes a full byte
            if (!sync_detected && received_byte == SYNC_BYTE) {
                //If we haven't synchronized yet and we've received the SYNC_BYTE
                sync_detected = 1;   
                bit_index = 0;       
                received_byte = 0;   
                message_index = 0;    /
            } else if (sync_detected && message_length == 0) {
                // synchronized and are now receiving the message length
                message_length = received_byte;  // Set the expected message length
                bit_index = 0;
                received_byte = 0;
            } else if (sync_detected && message_length > 0) {
               
                received_message[message_index++] = received_byte;  
                bit_index = 0;
                received_byte = 0;

                if (message_index >= message_length) {
                    // We've received the entire message
                    printf("[Received]: ");
                    for (int i = 0; i < message_length; i++) {
                        printf("%c", received_message[i]);  // Print each character
                    }
                    printf("\n");
                    // Reset for the next message
                    sync_detected = 0;
                    message_length = 0;
                    message_index = 0;
                }
            } else {
                bit_index = 0;
                received_byte = 0;
            }
        }
    }

    last_level = level;  // update last_level for the next callback
}

int main() {
    int gpio_handle = pigpio_start(NULL, NULL);  
    if (gpio_handle < 0) {
        fprintf(stderr, "Failed to initialize GPIO\n");
        return -1;
    }

    set_mode(gpio_handle, TX_PIN, PI_OUTPUT);  
    set_mode(gpio_handle, RX_PIN, PI_INPUT);   

    set_pull_up_down(gpio_handle, RX_PIN, PI_PUD_DOWN);

    // setting a glitch filter on RX_PIN to filter out noise shorter than 100 microseconds
    set_glitch_filter(gpio_handle, RX_PIN, 100);

    // Register the callback function for both rising and falling edges on RX_PIN
    callback_t *cb = callback_ex(gpio_handle, RX_PIN, EITHER_EDGE, rx_callback, NULL);

    char message[BUFFER_SIZE];  // Buffer to store the message to send
    while (1) {
        printf("Enter message (or 'exit' to quit): ");
        fgets(message, BUFFER_SIZE, stdin);        
        message[strcspn(message, "\n")] = '\0';    

        if (strcmp(message, "exit") == 0) break;    

        uint8_t length = strlen(message); 
        manchester_transmit(gpio_handle, TX_PIN, SYNC_BYTE);

        manchester_transmit(gpio_handle, TX_PIN, length);

        for (uint8_t i = 0; i < length; i++) {
            manchester_transmit(gpio_handle, TX_PIN, (uint8_t)message[i]);
        }

        printf("[Sent]: %s\n", message);

        time_sleep((length + 2) * BIT_DURATION_US * 8 / 1000000.0);
    }

    callback_cancel(cb);
    pigpio_stop(gpio_handle);
    return 0;
}
