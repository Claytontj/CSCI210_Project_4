
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// Structure to hold message details
typedef struct {
    char sender[50];
    char recipient[50];
    char content[200];
} Message;

// Graceful termination handler
void handleTermination(int signal) {
    printf("Server shutting down...\n");
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

int main() {
    int serverFd, recipientFd, placeholderFd;
    Message incomingMessage;

    // Signal handling setup
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleTermination);

    // Open the server FIFO for reading
    serverFd = open("serverFIFO", O_RDONLY);
    if (serverFd < 0) {
        perror("Failed to open serverFIFO for reading");
        exit(EXIT_FAILURE);
    }

    // Open the server FIFO for writing to prevent EOF on read
    placeholderFd = open("serverFIFO", O_WRONLY);
    if (placeholderFd < 0) {
        perror("Failed to open serverFIFO for writing");
        close(serverFd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Wait for incoming messages
        ssize_t bytesRead = read(serverFd, &incomingMessage, sizeof(incomingMessage));
        if (bytesRead != sizeof(incomingMessage)) {
            continue; // Skip invalid or partial messages
        }

        // Log the received message
        printf("Processing request from %s to deliver message \"%s\" to %s.\n",
               incomingMessage.sender, incomingMessage.content, incomingMessage.recipient);

        // Open the recipient's FIFO for writing
        recipientFd = open(incomingMessage.recipient, O_WRONLY);
        if (recipientFd < 0) {
            perror("Failed to open recipient FIFO");
            continue; // Skip this message and wait for the next
        }

        // Send the message to the recipient
        ssize_t bytesWritten = write(recipientFd, &incomingMessage, sizeof(incomingMessage));
        if (bytesWritten != sizeof(incomingMessage)) {
            perror("Failed to deliver message");
        } else {
            printf("Message successfully sent to %s.\n", incomingMessage.recipient);
        }

        // Close the recipient's FIFO
        close(recipientFd);
    }

    // Cleanup before exiting
    close(serverFd);
    close(placeholderFd);

    return 0;
}
