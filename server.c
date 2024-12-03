
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

// Define the structure to hold message details
typedef struct {
    char sender[50];    // Sender of the message
    char recipient[50]; // Recipient of the message
    char content[200];  // Message text
} Message;

// Graceful exit on receiving termination signals
void handleExit(int signal) {
    printf("Shutting down...\n");
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

int main() {
    int serverFd, recipientFd, placeholderFd; // File descriptors
    Message messageData;                      // Storage for incoming messages

    // Set up signal handlers
    signal(SIGPIPE, SIG_IGN);   // Ignore SIGPIPE to prevent crashes on broken pipes
    signal(SIGINT, handleExit); // Handle SIGINT (e.g., Ctrl+C) for graceful termination

    // Open the server FIFO for reading and writing
    serverFd = open("serverFIFO", O_RDONLY); // Open server FIFO for reading
    if (serverFd < 0) {
        perror("Failed to open server FIFO for reading");
        exit(EXIT_FAILURE);
    }

    placeholderFd = open("serverFIFO", O_WRONLY); // Open server FIFO for writing to keep it open
    if (placeholderFd < 0) {
        perror("Failed to open server FIFO for writing");
        close(serverFd);
        exit(EXIT_FAILURE);
    }

    // Main loop to process incoming messages
    while (1) {
        // Wait for a message to be read from the server FIFO
        ssize_t bytesRead = read(serverFd, &messageData, sizeof(Message));
        if (bytesRead != sizeof(Message)) {
            continue; // Skip if incomplete or invalid data
        }

        // Log the details of the received message
        printf("Message received from %s: \"%s\" for %s.\n",
               messageData.sender, messageData.content, messageData.recipient);

        // Open the recipient's FIFO for writing
        recipientFd = open(messageData.recipient, O_WRONLY);
        if (recipientFd < 0) {
            perror("Failed to open recipient FIFO");
            continue; // Skip this message if the recipient's FIFO cannot be opened
        }

        // Write the message to the recipient's FIFO
        ssize_t bytesWritten = write(recipientFd, &messageData, sizeof(Message));
        if (bytesWritten != sizeof(Message)) {
            perror("Failed to write message to recipient");
        }

        // Close the recipient FIFO
        close(recipientFd);
    }

    // Cleanup: Close file descriptors before exiting
    close(serverFd);
    close(placeholderFd);
    return 0;
}
