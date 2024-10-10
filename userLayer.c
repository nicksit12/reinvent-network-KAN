#include <stdio.h>
#include <string.h>
#include "network_layer.h"

int main() {
    // Initialize the network layer
    network_layer_init();

    char input_buf[256];
    uint8_t dest_ip[IP_ADDR_LEN];

    while (1) {
        // Ask the user for the destination IP
        printf("> Enter destination IP (format: xxx.xxx.xxx.xxx, or 'exit' to quit): ");
        fflush(stdout);

        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            break;
        }

        // Remove newline character from input
        size_t len = strlen(input_buf);
        if (len > 0 && input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
        }

        // Exit condition
        if (strcmp(input_buf, "exit") == 0) {
            break;
        }

        // Parse destination IP
        if (sscanf(input_buf, "%hhu.%hhu.%hhu.%hhu", &dest_ip[0], &dest_ip[1], &dest_ip[2], &dest_ip[3]) != 4) {
            printf("Invalid IP address format.\n");
            continue;
        }

        // Ask the user for the message to send
        printf("> Enter message: ");
        fflush(stdout);

        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            break;
        }

        // Remove newline character from message
        len = strlen(input_buf);
        if (len > 0 && input_buf[len - 1] == '\n') {
            input_buf[len - 1] = '\0';
        }

        // Send the packet using the network layer
        send_packet(dest_ip, (uint8_t*)input_buf, (uint8_t)strlen(input_buf));

        printf("[Sent]: %s to IP: %d.%d.%d.%d\n", input_buf, dest_ip[0], dest_ip[1], dest_ip[2], dest_ip[3]);
        fflush(stdout);

        usleep(100000); // Sleep to allow time for transmission
    }

    return 0;
}
