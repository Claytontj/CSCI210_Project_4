
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

void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main()
{
	int server;
	int target;
	int dummyfd;
	struct message req;

	// Ignore SIGPIPE to avoid termination when writing to a closed FIFO
	signal(SIGPIPE, SIG_IGN);

	// Handle SIGINT for graceful termination
	signal(SIGINT, terminate);

	// Open the server FIFO for reading and create a dummy write FD to keep it open
	server = open("serverFIFO", O_RDONLY);
	if (server < 0)
	{
		perror("Failed to open serverFIFO for reading");
		exit(EXIT_FAILURE);
	}
	dummyfd = open("serverFIFO", O_WRONLY);
	if (dummyfd < 0)
	{
		perror("Failed to open serverFIFO for writing");
		close(server);
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		// Read requests from serverFIFO
		ssize_t bytesRead = read(server, &req, sizeof(req));
		if (bytesRead < 0)
		{
			perror("Failed to read from serverFIFO");
			break;
		}
		else if (bytesRead == 0)
		{
			// End of file (FIFO closed by all writers)
			printf("Server FIFO closed by all writers. Exiting...\n");
			break;
		}

		printf("Received a request from %s to send the message \"%s\" to %s.\n", req.source, req.msg, req.target);

		// Open target FIFO and write the whole message struct to the target FIFO
		char targetFIFO[64];
		snprintf(targetFIFO, sizeof(targetFIFO), "%s", req.target);
		target = open(targetFIFO, O_WRONLY);
		if (target < 0)
		{
			perror("Failed to open target FIFO");
			continue; // Skip this message and wait for the next request
		}

		ssize_t bytesWritten = write(target, &req, sizeof(req));
		if (bytesWritten < 0)
		{
			perror("Failed to write to target FIFO");
		}
		else
		{
			printf("Message delivered to %s successfully.\n", req.target);
		}

		close(target); // Close target FIFO after writing
	}

	// Close FIFOs before exiting
	close(server);
	close(dummyfd);
	return 0;
}