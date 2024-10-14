#include <stdio.h>
#include <string.h>
#include "network_layer.h"

int main() {
    //initialize network layer
    network_layer_init();

    char input_buf[256];
    uint8_t dest_addr;

    while (1) {
        //ask the user for destination device
        printf("> Enter destination device (1-255, or 'exit' to quit): ");
        fflush(stdout);

        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            break;
        }

        //remove newline character from input
        size_t len = strlen(input_buf);
        if (len > 0 && input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
        }

        //an exit condition
        if (strcmp(input_buf, "exit") == 0) {
            break;
        }

        //convert destination input to a 1-byte address
        dest_addr = (uint8_t)atoi(input_buf);
        if (dest_addr < 1 || dest_addr > 255) {
            printf("Invalid address. Please enter a value between 1 and 255.\n");
            continue;
        }

        //ask the user for the message to send to device 
        printf("> Enter message: ");
        fflush(stdout);

        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            break;
        }

        //remove newline character from message
        len = strlen(input_buf);
        if (len > 0 && input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
        }

        //send the packet using the network layer
        send_packet(dest_addr, (uint8_t*)input_buf, (uint8_t)strlen(input_buf));

        printf("[Sent]: %s to Device: %d\n", input_buf, dest_addr);
        fflush(stdout);

        usleep(100000); //sleep to allow time for transmission
    }

    return 0;
}
