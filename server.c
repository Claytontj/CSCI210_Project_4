
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message
{
	char source[50];
	char target[50];
	char msg[200]; // message body
};

// Signal handler to terminate the program gracefully
void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main()
{
	int serverFd, targetFd, dummyFd;
	struct message req;

	// Ignore SIGPIPE to avoid crashes when writing to a closed FIFO
	signal(SIGPIPE, SIG_IGN);

	// Handle SIGINT to terminate the program gracefully
	signal(SIGINT, terminate);

	// Open server FIFO for reading requests
	serverFd = open("serverFIFO", O_RDONLY);
	if (serverFd < 0)
	{
		perror("Failed to open serverFIFO for reading");
		exit(EXIT_FAILURE);
	}

	// Open server FIFO for writing to keep it open
	dummyFd = open("serverFIFO", O_WRONLY);
	if (dummyFd < 0)
	{
		perror("Failed to open serverFIFO for writing");
		close(serverFd);
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		// Read requests from serverFIFO
		ssize_t bytesRead = read(serverFd, &req, sizeof(req));
		if (bytesRead < 0)
		{
			perror("Failed to read from serverFIFO");
			break;
		}
		else if (bytesRead == 0)
		{
			// Handle end of file if no writers are connected
			printf("Server FIFO closed by all writers. Exiting...\n");
			break;
		}

		// Log received request details
		printf("Received a request from %s to send the message \"%s\" to %s.\n", req.source, req.msg, req.target);

		// Open target FIFO for writing the message
		char targetFIFO[64];
		snprintf(targetFIFO, sizeof(targetFIFO), "%s", req.target);
		targetFd = open(targetFIFO, O_WRONLY);
		if (targetFd < 0)
		{
			perror("Failed to open target FIFO");
			continue; // Skip this request and wait for the next one
		}

		// Write the message to the target FIFO
		ssize_t bytesWritten = write(targetFd, &req, sizeof(req));
		if (bytesWritten < 0)
		{
			perror("Failed to write to target FIFO");
		}
		else
		{
			printf("Message delivered to %s successfully.\n", req.target);
		}

		// Close the target FIFO after writing
		close(targetFd);
	}

	// Clean up: Close server and dummy file descriptors
	close(serverFd);
	close(dummyFd);

	return 0;
}