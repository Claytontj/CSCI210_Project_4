#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define COMMAND_COUNT 13

extern char **environ;
char currentUser[50];

const char *validCommands[COMMAND_COUNT] = {
    "cp", "touch", "mkdir", "ls", "pwd", "cat", 
    "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"
};

typedef struct {
    char sender[50];
    char recipient[50];
    char message[200];
} ChatMessage;

void cleanup(int signal) {
    printf("Shutting down...\n");
    fflush(stdout);
    exit(EXIT_SUCCESS);
}

void dispatchMessage(const char *sender, const char *recipient, const char *text) {
    int serverFd = open("serverFIFO", O_WRONLY);
    if (serverFd < 0) {
        perror("Unable to connect to server FIFO");
        return;
    }

    ChatMessage msg;
    strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
    strncpy(msg.recipient, recipient, sizeof(msg.recipient) - 1);
    strncpy(msg.message, text, sizeof(msg.message) - 1);

    if (write(serverFd, &msg, sizeof(msg)) != sizeof(msg)) {
        perror("Failed to send message");
    }
    close(serverFd);
}

void *listenerThreadFunc(void *arg) {
    char fifoPath[64];
    snprintf(fifoPath, sizeof(fifoPath), "%s", currentUser);

    int userFd = open(fifoPath, O_RDONLY);
    if (userFd < 0) {
        perror("Unable to open user FIFO");
        pthread_exit(NULL);
    }

    ChatMessage incomingMsg;
    while (1) {
        if (read(userFd, &incomingMsg, sizeof(incomingMsg)) > 0) {
            printf("Message from [%s]: %s\n", incomingMsg.sender, incomingMsg.message);
        }
    }

    close(userFd);
    pthread_exit(NULL);
}

int isCommandValid(const char *command) {
    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(command, validCommands[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <username>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, cleanup);
    strncpy(currentUser, argv[1], sizeof(currentUser) - 1);

    pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, listenerThreadFunc, NULL) != 0) {
        perror("Failed to start message listener");
        exit(EXIT_FAILURE);
    }

    char inputLine[256];
    while (1) {
        fprintf(stderr, "shell> ");
        if (fgets(inputLine, sizeof(inputLine), stdin) == NULL)
            continue;

        if (strcmp(inputLine, "\n") == 0)
            continue;

        inputLine[strcspn(inputLine, "\n")] = '\0';

        char command[64], tempLine[256];
        strncpy(tempLine, inputLine, sizeof(tempLine) - 1);
        strncpy(command, strtok(inputLine, " "), sizeof(command) - 1);

        if (!isCommandValid(command)) {
            printf("Invalid command\n");
            continue;
        }

        if (strcmp(command, "sendmsg") == 0) {
            char *recipient = strtok(NULL, " ");
            char *content = strtok(NULL, "");

            if (!recipient) {
                printf("Error: No recipient specified\n");
                continue;
            }
            if (!content) {
                printf("Error: No message content provided\n");
                continue;
            }

            dispatchMessage(currentUser, recipient, content);
            continue;
        }

        if (strcmp(command, "exit") == 0) {
            break;
        }

        if (strcmp(command, "cd") == 0) {
            char *directory = strtok(NULL, " ");
            if (strtok(NULL, " ")) {
                printf("Error: Too many arguments for cd\n");
            } else if (chdir(directory) != 0) {
                perror("Failed to change directory");
            }
            continue;
        }

        if (strcmp(command, "help") == 0) {
            printf("Available commands:\n");
            for (int i = 0; i < COMMAND_COUNT; i++) {
                printf("  %s\n", validCommands[i]);
            }
            continue;
        }

        char *args[16];
        char *arg = strtok(tempLine, " ");
        int argCount = 0;
        while (arg && argCount < 15) {
            args[argCount++] = strdup(arg);
            arg = strtok(NULL, " ");
        }
        args[argCount] = NULL;

        posix_spawnattr_t spawnAttributes;
        posix_spawnattr_init(&spawnAttributes);

        pid_t pid;
        if (posix_spawnp(&pid, command, NULL, &spawnAttributes, args, environ) != 0) {
            perror("Failed to execute command");
        }

        if (waitpid(pid, NULL, 0) < 0) {
            perror("Error waiting for process");
        }

        posix_spawnattr_destroy(&spawnAttributes);
        for (int i = 0; i < argCount; i++) {
            free(args[i]);
        }
    }

    pthread_cancel(listenerThread);
    pthread_join(listenerThread, NULL);

    return 0;
}