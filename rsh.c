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

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep",
					"chmod", "diff", "cd", "exit",
					"help", "sendmsg"};

struct message
{
	char source[50];
	char target[50];
	char msg[200];
};

void terminate(int sig)
{
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

void sendmsg(char *user, char *target, char *msg)
{

	struct message deliverable;
	int serverFIFO = open("serverFIFO", O_WRONLY);

	if (serverFIFO == -1)
	{
		perror("Error opening server FIFO");
		exit(EXIT_FAILURE);
	}

	// Populate the message struct
	snprintf(deliverable.source, sizeof(deliverable.source), "%s", user);
	snprintf(deliverable.target, sizeof(deliverable.target), "%s", target);
	snprintf(deliverable.msg, sizeof(deliverable.msg), "%s", msg);

	// Write to FIFO
	ssize_t bytesWritten = write(serverFIFO, &deliverable, sizeof(struct message));
	if (bytesWritten != sizeof(struct message))
	{
		perror("Error writing to server FIFO");
		close(serverFIFO);
		exit(EXIT_FAILURE);
	}

	close(serverFIFO);
}

void *messageListener(void *arg)
{

	struct message inMessage;

	int clientFD;
	clientFD = open(arg, O_RDONLY);

	while (1)
	{
		if (read(clientFD, &inMessage, sizeof(struct message)) != sizeof(struct message))
		{
			continue;
		}

		printf("Incoming message from %s: %s\n", inMessage.source, inMessage.msg);
	}
	pthread_exit((void *)0);
}

int isAllowed(const char *cmd)
{
	int i;
	for (i = 0; i < N; i++)
	{
		if (strcmp(cmd, allowed[i]) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	pid_t pid;
	char **cargv;
	char *path;
	char line[256];
	int status;
	posix_spawnattr_t attr;

	if (argc != 2)
	{
		printf("Usage: ./rsh <username>\n");
		exit(1);
	}
	signal(SIGINT, terminate);

	strcpy(uName, argv[1]);

	pthread_t listThread;
	pthread_create(&listThread, NULL, messageListener, argv[1]);

	while (1)
	{

		fprintf(stderr, "rsh>");

		if (fgets(line, 256, stdin) == NULL)
			continue;

		if (strcmp(line, "\n") == 0)
			continue;

		line[strlen(line) - 1] = '\0';

		char cmd[256];
		char line2[256];
		strcpy(line2, line);
		strcpy(cmd, strtok(line, " "));

		if (!isAllowed(cmd))
		{
			printf("NOT ALLOWED!\n");
			continue;
		}

		if (strcmp(cmd, "sendmsg") == 0)
		{
			char *targetUser = strtok(NULL, " ");
			if (!targetUser)
			{
				fprintf(stderr, "Error: Target user must be specified.\n");
				continue;
			}

			char *message = strtok(NULL, "");
			if (!message)
			{
				fprintf(stderr, "Error: Message content must be provided.\n");
				continue;
			}

			sendmsg(argv[1], targetUser, message);
			continue;
		}

		if (strcmp(cmd, "exit") == 0)
			break;

		if (strcmp(cmd, "cd") == 0)
		{
			char *targetDir = strtok(NULL, " ");
			if (strtok(NULL, " ") != NULL)
			{
				printf("-rsh: cd: too many arguments\n");
			}
			else
			{
				chdir(targetDir);
			}
			continue;
		}

		if (strcmp(cmd, "help") == 0)
		{
			printf("The allowed commands are:\n");
			for (int i = 0; i < N; i++)
			{
				printf("%d: %s\n", i + 1, allowed[i]);
			}
			continue;
		}

		cargv = (char **)malloc(sizeof(char *));
		cargv[0] = (char *)malloc(strlen(cmd) + 1);
		path = (char *)malloc(9 + strlen(cmd) + 1);
		strcpy(path, cmd);
		strcpy(cargv[0], cmd);

		char *attrToken = strtok(line2, " ");
		attrToken = strtok(NULL, " ");
		int n = 1;
		while (attrToken != NULL)
		{
			n++;
			cargv = (char **)realloc(cargv, sizeof(char *) * n);
			cargv[n - 1] = (char *)malloc(strlen(attrToken) + 1);
			strcpy(cargv[n - 1], attrToken);
			attrToken = strtok(NULL, " ");
		}
		cargv = (char **)realloc(cargv, sizeof(char *) * (n + 1));
		cargv[n] = NULL;

		posix_spawnattr_init(&attr);

		if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0)
		{
			perror("spawn failed");
			exit(EXIT_FAILURE);
		}

		if (waitpid(pid, &status, 0) == -1)
		{
			perror("waitpid failed");
			exit(EXIT_FAILURE);
		}

		posix_spawnattr_destroy(&attr);
	}
	return 0;
}