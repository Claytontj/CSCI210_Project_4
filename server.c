
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// Define the structure for messages
struct message {
    char source[50];  // Sender of the message
    char target[50];  // Recipient of the message
    char msg[200];    // Message content
};

// Signal handler for termination (e.g., when SIGINT is received)
void terminate(int sig) {
    printf("Exiting....\n");  // Inform the user about termination
    fflush(stdout);          // Flush the output buffer
    exit(0);                 // Exit the program gracefully
}

int main() {
    int server;     // File descriptor for the server FIFO
    int target;     // File descriptor for the target user FIFO
    int dummyfd;    // Dummy file descriptor to keep the server FIFO open for writing
    struct message req;  // Structure to hold the incoming message request

    // Ignore SIGPIPE to prevent termination when writing to a closed FIFO
    signal(SIGPIPE, SIG_IGN);

    // Handle SIGINT (Ctrl+C) to terminate the program gracefully
    signal(SIGINT, terminate);

    // Open the server FIFO for reading
    server = open("serverFIFO", O_RDONLY);
    if (server < 0) {
        perror("Failed to open serverFIFO for reading");
        exit(EXIT_FAILURE);
    }

    // Open the server FIFO for writing to keep it open
    dummyfd = open("serverFIFO", O_WRONLY);
    if (dummyfd < 0) {
        perror("Failed to open serverFIFO for writing");
        close(server);  // Close the read end if write fails
        exit(EXIT_FAILURE);
    }

    // Infinite loop to process incoming messages
    while (1) {
        // Read a message from the server FIFO
        if (read(server, &req, sizeof(struct message)) != sizeof(struct message)) {
            // If the read operation fails or doesn't read the expected size, skip this iteration
            continue;
        }

        // Log the received message details
        printf("Received a request from %s to send the message \"%s\" to %s.\n",
               req.source, req.msg, req.target);

        // Open the recipient's FIFO for writing
        target = open(req.target, O_WRONLY);
        if (target < 0) {
            // If the target FIFO cannot be opened, log an error and skip this message
            perror("Failed to open target FIFO");
            continue;
        }

        // Write the message to the recipient's FIFO
        if (write(target, &req, sizeof(struct message)) != sizeof(struct message)) {
            // If the write operation fails, log an error
            perror("Failed to write to target FIFO");
        }

        // Close the recipient's FIFO after sending the message
        close(target);
    }

    // Close the server FIFOs before exiting (never reached in this infinite loop)
    close(server);
    close(dummyfd);

    return 0;
}
