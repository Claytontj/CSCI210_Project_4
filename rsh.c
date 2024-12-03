#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

// List of allowed commands
char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", 
                    "grep", "chmod", "diff", "cd", "exit", 
                    "help", "sendmsg"};

// Structure to store message details
struct message {
    char source[50];
    char target[50];
    char msg[200];
};

// Signal handler to terminate the program
void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

// Function to send a message
void sendmsg(char *user, char *target, char *msg) {
    int serverFIFO = open("serverFIFO", O_WRONLY); // Open the server FIFO for writing
    if (serverFIFO < 0) {
        perror("Failed to open server FIFO");
        return;
    }

    struct message deliverable;
    strcpy(deliverable.source, user);
    strcpy(deliverable.target, target);
    strcpy(deliverable.msg, msg);

    // Write the message to the server FIFO
    if (write(serverFIFO, &deliverable, sizeof(struct message)) != sizeof(struct message)) {
        printf("Failed to write to server FIFO\n");
    }

    close(serverFIFO); // Close the FIFO after writing
}

// Thread function to listen for incoming messages
void *messageListener(void *arg) {
    int clientFD = open((char *)arg, O_RDONLY); // Open the user's FIFO for reading
    if (clientFD < 0) {
        perror("Failed to open user FIFO");
        pthread_exit(NULL);
    }

    struct message inMessage;
    while (1) {
        // Read incoming messages
        if (read(clientFD, &inMessage, sizeof(struct message)) != sizeof(struct message)) {
            continue;
        }

        // Print the received message
        printf("Incoming message from %s: %s\n", inMessage.source, inMessage.msg);
    }

    close(clientFD);
    pthread_exit(NULL);
}

// Function to check if a command is allowed
int isAllowed(const char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1; // Command is allowed
        }
    }
    return 0; // Command is not allowed
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    signal(SIGINT, terminate); // Set up signal handler
    strcpy(uName, argv[1]);

    // Create a thread to listen for messages
    pthread_t listThread;
    pthread_create(&listThread, NULL, messageListener, argv[1]);

    char line[256];

    while (1) {
        fprintf(stderr, "rsh> "); // Display prompt

        if (fgets(line, sizeof(line), stdin) == NULL) {
            continue; // Skip empty input
        }

        if (strcmp(line, "\n") == 0) {
            continue; // Ignore newline-only input
        }

        line[strlen(line) - 1] = '\0'; // Remove trailing newline

        char cmd[256], line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " ")); // Extract the command

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue; // Skip disallowed commands
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            // Extract target user and message
            char *tar = strtok(NULL, " ");
            if (tar == NULL) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *msg = strtok(NULL, "");
            if (msg == NULL) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            sendmsg(argv[1], tar, msg); // Send the message
            continue;
        }

        if (strcmp(cmd, "exit") == 0) {
            break; // Exit the program
        }

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else if (chdir(targetDir) != 0) {
                perror("Failed to change directory");
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        // Handle other commands
        char **cargv = malloc(sizeof(char *));
        cargv[0] = malloc(strlen(cmd) + 1);
        strcpy(cargv[0], cmd);

        char *attrToken = strtok(line2, " ");
        attrToken = strtok(NULL, " ");
        int n = 1;

        while (attrToken != NULL) {
            n++;
            cargv = realloc(cargv, sizeof(char *) * n);
            cargv[n - 1] = malloc(strlen(attrToken) + 1);
            strcpy(cargv[n - 1], attrToken);
            attrToken = strtok(NULL, " ");
        }
        cargv = realloc(cargv, sizeof(char *) * (n + 1));
        cargv[n] = NULL;

        char *path = malloc(strlen(cmd) + 1);
        strcpy(path, cmd);

        posix_spawnattr_t attr;
        posix_spawnattr_init(&attr);

        pid_t pid;
        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("Failed to spawn process");
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("Failed to wait for process");
        }

        posix_spawnattr_destroy(&attr);

        // Free allocated memory
        for (int i = 0; i < n; i++) {
            free(cargv[i]);
        }
        free(cargv);
        free(path);
    }

    return 0;
}
