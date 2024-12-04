#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
// Define the structure to hold message details
struct message
{
    char source[50]; // Sender of the message
    char target[50]; // Recipient of the message
    char msg[200];  // Message text
};
// Graceful exit on receiving termination signals
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

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);
    server = open("serverFIFO", O_RDONLY);
    dummyfd = open("serverFIFO", O_WRONLY);

    while (1)
    {

        if (read(server, &req, sizeof(struct message)) != sizeof(struct message))
        {
            continue;
        }

        printf("Received a request from %s to send the message %s to %s.\n", req.source, req.msg, req.target);

        target = open(req.target, O_WRONLY);
        write(target, &req, sizeof(struct message));

        close(target);
    }
    close(server);
    close(dummyfd);
    return 0;
}