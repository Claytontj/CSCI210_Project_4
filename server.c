
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// Define the message structure
typedef struct {
    char sender[50];
    char recipient[50];
    char content[200];
} Message;

// Handle termination signals
void terminate(int sig) {
    printf("Server shutting down...\n");
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

int main() {
    int serverFd, recipientFd, keepAliveFd;
    Message incoming;

    // Set up signal handling
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    // Open server FIFO for reading
    serverFd = open("serverFIFO", O_RDONLY);
    if (serverFd < 0) {
        perror("Failed to open serverFIFO for reading");
        exit(EXIT_FAILURE);
    }

    // Open server FIFO for writing to keep it alive
    keepAliveFd = open("serverFIFO", O_WRONLY);
    if (keepAliveFd < 0) {
        perror("Failed to open serverFIFO for writing");
        close(serverFd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Wait for incoming message
        ssize_t bytesRead = read(serverFd, &incoming, sizeof(incoming));
        if (bytesRead != sizeof(incoming)) {
            continue;
        }

        // Log the incoming message
        printf("Processing message from %s: \"%s\" to %s\n", incoming.sender, incoming.content, incoming.recipient);

        // Open the recipient FIFO
        recipientFd = open(incoming.recipient, O_WRONLY);
        if (recipientFd < 0) {
            perror("Unable to open recipient FIFO");
            continue;
        }

        // Deliver the message to the recipient
        ssize_t bytesWritten = write(recipientFd, &incoming, sizeof(incoming));
        if (bytesWritten != sizeof(incoming)) {
            perror("Failed to deliver message");
        } else {
            printf("Message successfully delivered to %s.\n", incoming.recipient);
        }

        // Close recipient FIFO
        close(recipientFd);
    }

    // Cleanup resources
    close(serverFd);
    close(keepAliveFd);
    return 0;
}
