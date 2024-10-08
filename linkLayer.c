#include <pigpiod_if2.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 128

#define BIT_DURATION_US 5000

static int rx_pins[] = {26, 24, 22, 20};

static int tx_pins[] = {27, 25, 23, 21};

static int gpio_handle = -1;

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

// Array to hold the state of each port
static ChannelState port_states[4];

// Buffer to hold the final received message
static uint8_t received_msg[BUFFER_SIZE];

// Function to map a GPIO pin number to the corresponding port index
static int gpio_to_port(unsigned gpio_pin) {
    for (int i = 0; i < 4; i++) {
        if (rx_pins[i] == (int)gpio_pin) {
            return i;
        }
    }
    return -1; 
}

//  reset the state of a communication channel (port)
static void reset_channel(ChannelState *port_state) {
    printf("Resetting channel\n");
    port_state->half_bit_signal = 0;
    port_state->sync_detected = 0;
    port_state->bit_pos = 0;
    port_state->msg_pos = 0;
    memset(port_state->bit_buffer, 0, sizeof(port_state->bit_buffer));
    memset(port_state->msg_buffer, 0, sizeof(port_state->msg_buffer));
    port_state->margin = BIT_DURATION_US;
}

// convert received bits into a character and assemble the message
static void bit_to_char(int port_index) {
    ChannelState *port_state = &port_states[port_index];
    uint8_t full_byte = 0;

    // combine bits into a single byte
    printf("Converting bits to char on port %d: ", port_index);
    for (int i = 0; i < 8; i++) {
        printf("%d", port_state->bit_buffer[i]);
        full_byte |= port_state->bit_buffer[i] << (7 - i);
    }
    printf(" => 0x%02X\n", full_byte);

    // put the byte in the message buffer
    port_state->msg_buffer[port_state->msg_pos++] = full_byte;

    // check if the entire message has been received
    if (port_state->msg_pos == port_state->msg_buffer[0] + 1) {
        // extract the checksum from the message
        uint8_t received_checksum = port_state->msg_buffer[port_state->msg_pos - 1];

        // Calculate the checksum of the received data taking out the length and checksum bytes
        uint8_t calculated_checksum = 0;
        for (int i = 1; i < port_state->msg_pos - 1; i++) {
            calculated_checksum += port_state->msg_buffer[i];
        }
        printf("Received checksum: 0x%02X, Calculated checksum: 0x%02X\n", received_checksum, calculated_checksum);

        // verify the checksum for error detection
        if (calculated_checksum == received_checksum) {
            //Copy the valid message to the received_msg buffer
            memcpy(received_msg, port_state->msg_buffer, port_state->msg_pos);

            // Process the received message 
            process_message(received_msg, port_index);
        } else {
            // If checksum does not match, discard the message
            printf("\nChecksum error on Port %d. Message discarded.\n", port_index);
            fflush(stdout);
        }
        // Reset the channel state for the next message
        reset_channel(port_state);
    }

    // Reset bit buffer for the next byte
    port_state->bit_pos = 0;
    memset(port_state->bit_buffer, 0, sizeof(port_state->bit_buffer));
}

// Callback function triggered on edge detection for receiving data
static void rx_callback(int pi, unsigned gpio, unsigned level, uint32_t tick) {
    int port_index = gpio_to_port(gpio);
    if (port_index == -1) return;

    ChannelState *port_state = &port_states[port_index];

    //calculate the time difference between the current and previous signal edges
    uint32_t time_diff = (tick >= port_state->prev_tick) ? (tick - port_state->prev_tick)
                                                                  : ((UINT32_MAX - port_state->prev_tick) + tick);
    // calculate the ratio of the time difference to the expected bit duration
    float margin_ratio = (float)time_diff / port_state->margin;

    printf("RX Callback on port %d: level=%d, tick=%u, time_diff=%u, margin_ratio=%.2f\n", port_index, level, tick, time_diff, margin_ratio);

    if (port_state->sync_detected) {
        printf("Sync detected on port %d\n", port_index);
        if (margin_ratio > 0.8f && margin_ratio < 1.2f) {
            // Full bit duration detected, store the level as a bit
            printf("full bit detected on link %d\n", port_index);
            port_state->bit_buffer[port_state->bit_pos++] = level;
        } else if (margin_ratio > 0.4f && margin_ratio < 0.6f) {
            // Half bit duration detected, handle Manchester encoding
            printf("Half bit detected on link %d\n", port_index);
            if (port_state->half_bit_signal == 0) {
                port_state->half_bit_signal = 1;
            } else {
                // Combine two half bits to form a full bit
                port_state->bit_buffer[port_state->bit_pos++] = level;
                port_state->half_bit_signal = 0;
            }
        } else {
            // If timing is off, reset the channel to resynchronize
            printf("Timing error on port %d, resetting channel\n", port_index);
            reset_channel(port_state);
        }
        // If 8 bits have been collected convert them into a byte
        if (port_state->bit_pos == 8) {
            bit_to_char(port_index);
        }
    } else {
        //  synchronization pattern to start message reception
        if (margin_ratio > 0.8f && margin_ratio < 1.2f && level == 1) {
            port_state->sync_detected = 1;
            printf("Sync pattern detected on port %d\n", port_index);
        }
    }
    // Update the timestamp of the last signal edge
    port_state->prev_tick = tick;
}

// Function to transmit data using Manchester encoding over the network
void manchester_transmit(int port, uint8_t *data, uint8_t len) {
    int32_t gpio_pin = 0;

    if (port >= 0 && port < 4) {
        // Transmit on a specific port
        gpio_pin = 1 << tx_pins[port];
    } else {
        //  to all ports
        for (int i = 0; i < 4; i++) {
            gpio_pin |= (1 << tx_pins[i]);
        }
    }
    // calc checksum for error detection
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < len; i++) {
        checksum += data[i];
    }

    printf("Transmitting data on link %d: ", port);
    for (uint8_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("Checksum: %02X\n", checksum);

    // Clear any existing waveforms
    wave_clear(gpio_handle);

    // Calculate the number of pulses required for the transmission
    int num_pulses = 3 + (16 * (len + 2)); // Including length and checksum bytes

    gpioPulse_t pulses[num_pulses];
    int pulse_idx = 0;

    pulses[pulse_idx].gpioOn = gpio_pin;  // High signal
    pulses[pulse_idx].gpioOff = 0;
    pulses[pulse_idx].usDelay = BIT_DURATION_US / 2;
    pulse_idx++;

    pulses[pulse_idx].gpioOn = 0;         // Low signal
    pulses[pulse_idx].gpioOff = gpio_pin;
    pulses[pulse_idx].usDelay = BIT_DURATION_US / 2;
    pulse_idx++;

    pulses[pulse_idx].gpioOn = gpio_pin;  // High signal for synchronization
    pulses[pulse_idx].gpioOff = 0;
    pulses[pulse_idx].usDelay = BIT_DURATION_US * 2;
    pulse_idx++;

    // Encode and transmit each byte using Manchester encoding
    for (uint8_t i = 0; i <= len + 1; i++) {
        uint8_t byte_data;
        if (i == 0) {
            byte_data = len + 1; // First byte is the length including checksum
        } else if (i == len + 1) {
            byte_data = checksum; // Last byte is the checksum
        } else {
            byte_data = data[i - 1]; // Data bytes
        }
        printf("Transmitting byte %d: 0x%02X\n", i, byte_data);
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t bit_value = (byte_data >> bit) & 1;
            printf("Bit %d: %d\n", bit, bit_value);

            if (bit_value == 1) {
                pulses[pulse_idx].gpioOn = 0;          // Low signal
                pulses[pulse_idx].gpioOff = gpio_pin;
                pulses[pulse_idx].usDelay = BIT_DURATION_US / 2;
                pulse_idx++;

                pulses[pulse_idx].gpioOn = gpio_pin;   // High signal
                pulses[pulse_idx].gpioOff = 0;
                pulses[pulse_idx].usDelay = BIT_DURATION_US / 2;
                pulse_idx++;
            } else {
                pulses[pulse_idx].gpioOn = gpio_pin;   // High signal
                pulses[pulse_idx].gpioOff = 0;
                pulses[pulse_idx].usDelay = BIT_DURATION_US / 2;
                pulse_idx++;

                pulses[pulse_idx].gpioOn = 0;          // Low signal
                pulses[pulse_idx].gpioOff = gpio_pin;
                pulses[pulse_idx].usDelay = BIT_DURATION_US / 2;
                pulse_idx++;
            }
        }
    }
    printf("Total pulses: %d\n", pulse_idx);

    wave_add_generic(gpio_handle, pulse_idx, pulses);

    int wave_id = wave_create(gpio_handle);
    printf("Wave ID: %d\n", wave_id);
    wave_send_once(gpio_handle, wave_id);
    usleep(BIT_DURATION_US * (8 * (len + 2) + 11));
}

// Function to process received messages
void process_message(uint8_t* msg, int port) {
    uint8_t msg_len = msg[0] - 1; // Adjust length to exclude checksum byte

    printf("\nOn Port %d Received: ", port);
    for (int i = 1; i <= msg_len; i++) {
        printf("%c", msg[i]);
    }
    printf("\n");
    fflush(stdout);

    printf("Broadcasting received message from port %d\n", port);
    manchester_transmit(-1, &msg[1], msg_len);
    printf("[Broadcasted]: ");
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
        uint8_t checksum = 0;
        for (size_t i = 0; i < len; i++) {
            checksum += input_buf[i];
        }
        uint8_t msg_buffer[BUFFER_SIZE];
        msg_buffer[0] = len + 1; 
        memcpy(&msg_buffer[1], input_buf, len);
        msg_buffer[len + 1] = checksum;

        printf("Processing user input message\n");
        process_message(msg_buffer, -1);
        usleep(100000); 
    }
    printf("Stopping pigpio\n");
    pigpio_stop(gpio_handle);
    return 0;
}
