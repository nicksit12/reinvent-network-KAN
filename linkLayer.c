#include <pigpiod_if2.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
//#include "linkLayer.h"

#define BUFFER_SIZE 128
#define BIT_DURATION_US 5000

int rx_pins[] = {26, 24, 22, 20};
int tx_pins[] = {27, 25, 23, 21};
int gpio_handle = -1;

// Structure to maintain the state of each communication link (port)
typedef struct {
    uint8_t half_bit_signal;        // Flag for detecting half bits in Manchester decoding
    uint8_t sync_detected;          // Flag to indicate if synchronization has been detected
    uint32_t prev_tick;             // Timestamp of the previous GPIO event
    uint32_t margin;                // Time margin for timing comparisons
    uint8_t bit_buffer[8];          // Buffer to store bits of a byte during reception
    int bit_pos;                    // Current position in the bit buffer
    uint8_t msg_buffer[BUFFER_SIZE];// Buffer to store the received message
    int msg_pos;                    // Current position in the message buffer
} ChannelState;

// Reimplement the callback function 
typedef void (*msg_callback_t)(uint8_t* msg, int ch);
static msg_callback_t user_msg_handler;

// Array to hold the state of each port
ChannelState port_states[4];

// Buffer to hold the final received message
uint8_t received_msg[BUFFER_SIZE];

// Function to map a GPIO pin number to the corresponding port index
static int gpio_to_port(unsigned gpio_pin) {
    for (int i = 0; i < 4; i++) {
        if (rx_pins[i] == (int)gpio_pin) {
            return i;
        }
    }
    return -1; 
}

// Reset the state of a communication channel (port)
static void reset_channel(ChannelState *ch_state) {
    ch_state->half_bit_signal = 0;
    ch_state->sync_detected = 0;
    ch_state->bit_pos = 0;
    ch_state->msg_pos = 0;
    memset(ch_state->bit_buffer, 0, sizeof(ch_state->bit_buffer));
    memset(ch_state->msg_buffer, 0, sizeof(ch_state->msg_buffer));
    ch_state->margin = BIT_DURATION_US;
}

static uint8_t compute_checksum(uint8_t *data, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(sum % 256);
}

// Convert received bits into a character and assemble the message
static void bit_to_char(int ch_index) {
    ChannelState *ch_state = &port_states[ch_index];
    uint8_t full_byte = 0;

    printf("Converting bits to char on port %d: ", ch_index);

    for (int i = 0; i < 8; i++) {
        full_byte |= ch_state->bit_buffer[i] << (7 - i);
    }

    printf(" => 0x%02X\n", full_byte);

    ch_state->msg_buffer[ch_state->msg_pos++] = full_byte;

    uint8_t expected_len = ch_state->msg_buffer[0]; // Number of data bytes

    if (ch_state->msg_pos == expected_len + 2) {
        int msg_len = ch_state->msg_pos;
        for (int j = 0; j < msg_len; j++) {
            received_msg[j] = ch_state->msg_buffer[j];
        }

        uint8_t received_checksum = received_msg[expected_len + 1];
        uint8_t computed_checksum = compute_checksum(&received_msg[1], expected_len);

        printf("msg_len is: %d \n", msg_len);    
        printf("Received checksum: 0x%02x\n", received_checksum);
        printf("Computed checksum: 0x%02x\n", computed_checksum);

        if (computed_checksum == received_checksum) {
            // Checksum matches
            if (user_msg_handler != NULL) {
                user_msg_handler(received_msg, ch_index);
            }
        } else {
            // Checksum error
            printf("\n[Port %d] Checksum mismatch. Discarding message.\n", ch_index);
            fflush(stdout);
        }
        reset_channel(ch_state);
    }

    ch_state->bit_pos = 0;
    memset(ch_state->bit_buffer, 0, sizeof(ch_state->bit_buffer));
}

// Callback function triggered on edge detection for receiving data
static void rx_callback(int pi, unsigned gpio, unsigned level, uint32_t tick) {
    int ch_index = gpio_to_port(gpio); 
    if (ch_index == -1) return;

    ChannelState *ch_state = &port_states[ch_index]; 

    // Calculate the time difference between the current and previous signal edges
    uint32_t time_diff = (tick >= ch_state->prev_tick) ? (tick - ch_state->prev_tick) 
                                                        : ((UINT32_MAX - ch_state->prev_tick) + tick);
    // Calculate the ratio of the time difference to the expected bit duration
    float margin_ratio = (float)time_diff / ch_state->margin; 

    printf("RX Callback on port %d: level=%d, tick=%u, time_diff=%u, margin_ratio=%.2f\n", 
           ch_index, level, tick, time_diff, margin_ratio);

    if (ch_state->sync_detected) { 
        printf("Sync detected on port %d\n", ch_index);

        if (margin_ratio > 0.8f && margin_ratio < 1.2f) { 
            // Full bit duration detected, store the level as a bit
            printf("full bit detected on link %d\n", ch_index);
            ch_state->bit_buffer[ch_state->bit_pos++] = level; 
        } else if (margin_ratio > 0.4f && margin_ratio < 0.6f) {
            // Half bit duration detected, handle Manchester encoding
            printf("Half bit detected on link %d\n", ch_index);
            if (ch_state->half_bit_signal == 0) {
                ch_state->half_bit_signal = 1; 
            } else {
                // Combine two half bits to form a full bit
                ch_state->bit_buffer[ch_state->bit_pos++] = level; 
                ch_state->half_bit_signal = 0;
            }
        } else { 
            // If timing is off, reset the channel to resynchronize
            printf("Timing error on port %d, resetting channel\n", ch_index);
            reset_channel(ch_state); 
        }

        // If 8 bits have been collected, convert them into a byte
        if (ch_state->bit_pos == 8) { 
            bit_to_char(ch_index); 
        }
    } else {
        // Detect synchronization pattern to start message reception
        if (margin_ratio > 0.8f && margin_ratio < 1.2f && level == 1) { 
            ch_state->sync_detected = 1; 
            printf("Sync pattern detected on port %d\n", ch_index);
        }
    }

    // Update the timestamp of the last signal edge
    ch_state->prev_tick = tick;
}

// Function to transmit data using Manchester encoding over the network
void manchester_transmit(int ch, uint8_t *data, uint8_t len) {
    int32_t gpio_pin = 0;

    if (ch >= 0 && ch < 4) {
        // Transmit on a specific port
        gpio_pin = 1 << tx_pins[ch];
    } else {
        // Transmit to all ports
        for (int i = 0; i < 4; i++) {
            gpio_pin |= (1 << tx_pins[i]);
        }
    }

    // Clear any existing waveforms
    wave_clear(gpio_handle);

    // Calculate checksum for error detection
    uint8_t checksum = compute_checksum(data, len);
    int total_bytes = len + 2; // Length byte + data bytes + checksum byte

    int num_pulses = 3 + (16 * total_bytes);

    gpioPulse_t pulses[num_pulses];
    int pulse_idx = 0;

    // Sync pulses
    pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = gpio_pin, .gpioOff = 0, .usDelay = BIT_DURATION_US / 2};
    pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = 0, .gpioOff = gpio_pin, .usDelay = BIT_DURATION_US};
    pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = gpio_pin, .gpioOff = 0, .usDelay = BIT_DURATION_US / 2};

    printf("Transmitting data on link %d: ", ch);
    for (uint8_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("Checksum: %02X\n", checksum);

    for (int i = 0; i < total_bytes; i++) {
        uint8_t byte_data;
        if (i == 0) {
            byte_data = len; 
        } else if (i == total_bytes - 1) {
            byte_data = checksum; // Checksum byte
        } else {
            byte_data = data[i - 1]; // Data bytes
        }
        printf("Transmitting byte %d: 0x%02X\n", i, byte_data);
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t bit_val = (byte_data >> bit) & 1;
            printf("Bit %d: %d\n", bit, bit_val);

            if (bit_val == 1) {
                // Transmit '1' bits (Manchester encoding)
                pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = 0, .gpioOff = gpio_pin, .usDelay = BIT_DURATION_US / 2};
                pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = gpio_pin, .gpioOff = 0, .usDelay = BIT_DURATION_US / 2};
            } else {
                // Transmit '0' bits
                pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = gpio_pin, .gpioOff = 0, .usDelay = BIT_DURATION_US / 2};
                pulses[pulse_idx++] = (gpioPulse_t){.gpioOn = 0, .gpioOff = gpio_pin, .usDelay = BIT_DURATION_US / 2};
            }
        }
    }

    printf("Total pulses: %d\n", pulse_idx);

    wave_add_generic(gpio_handle, pulse_idx, pulses);

    int wave_id = wave_create(gpio_handle);
    printf("Wave ID: %d\n", wave_id);
    wave_send_once(gpio_handle, wave_id);
    usleep(BIT_DURATION_US * (2 + (8 * total_bytes) + 2));
}

void print_callback(uint8_t* msg, int ch) {
    uint8_t msg_len = msg[0]; // Number of data bytes

    printf("\nOn Port: %d Received: ", ch);
    for (int i = 1; i <= msg_len; i++) {
        printf("%c", msg[i]);
    }
    printf("\n");
    fflush(stdout);
}

int main() {
    printf("Starting program\n");
    gpio_handle = pigpio_start(NULL, NULL);
    if (gpio_handle < 0) {
        fprintf(stderr, "Failed to start pigpio\n");
        return 1;
    }
    printf("pigpio started with handle %d\n", gpio_handle);

    user_msg_handler = print_callback;
    for (int i = 0; i < 4; i++) {
        printf("Initializing port %d: rx_pin=%d, tx_pin=%d\n", i, rx_pins[i], tx_pins[i]);
        reset_channel(&port_states[i]);
        set_mode(gpio_handle, rx_pins[i], PI_INPUT);   // Set RX pin as input
        set_mode(gpio_handle, tx_pins[i], PI_OUTPUT);  // Set TX pin as output
        gpio_write(gpio_handle, tx_pins[i], 1);        // Set TX pin high
        callback(gpio_handle, rx_pins[i], EITHER_EDGE, rx_callback); // Set up callback for RX pin
    }
    usleep(100000);

    while (1) {
        char input_buf[128];
        printf("> Enter message: ");
        fflush(stdout);
        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            break;
        }
        size_t len = strlen(input_buf);
        if (len > 0 && input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
            len--;
        }
        if (strcmp(input_buf, "exit") == 0) {
            break;
        }

        printf("Sent: %s\n", input_buf);

        manchester_transmit(-1, (uint8_t*)&input_buf[0], len);
        printf("Broadcasted: "); 
        for (int i = 0; i < len; i++) {
            printf("%c", input_buf[i]);
        }
        printf("\n");
        fflush(stdout); 

        usleep(100000);
    }

    printf("Stopping pigpio\n");
    pigpio_stop(gpio_handle);
    return 0;
}
