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

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"};

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
	int serverFd = open("serverFIFO", O_WRONLY);
	if (serverFd < 0)
	{
		perror("Error opening serverFIFO");
		return;
	}

	struct message m;
	strncpy(m.source, user, sizeof(m.source) - 1);
	strncpy(m.target, target, sizeof(m.target) - 1);
	strncpy(m.msg, msg, sizeof(m.msg) - 1);

	if (write(serverFd, &m, sizeof(m)) < 0)
	{
		perror("Error writing to serverFIFO");
	}
	close(serverFd);
}

void *messageListener(void *arg)
{
	char fifoName[50];
	snprintf(fifoName, sizeof(fifoName), "%s", uName);

	int userFd = open(fifoName, O_RDONLY);
	if (userFd < 0)
	{
		perror("Error opening user's FIFO");
		pthread_exit((void *)1);
	}

	struct message m;
	while (1)
	{
		ssize_t bytesRead = read(userFd, &m, sizeof(m));
		if (bytesRead > 0)
		{
			printf("Incoming message from [%s]: %s\n", m.source, m.msg);
		}
	}
	close(userFd);
	pthread_exit(NULL);
}

int isAllowed(const char *cmd)
{
	for (int i = 0; i < N; i++)
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
	if (argc != 2)
	{
		printf("Usage: ./rsh <username>\n");
		exit(1);
	}

	signal(SIGINT, terminate);
	strncpy(uName, argv[1], sizeof(uName) - 1);

	pthread_t listenerThread;
	if (pthread_create(&listenerThread, NULL, messageListener, NULL) != 0)
	{
		perror("Error creating message listener thread");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		fprintf(stderr, "rsh> ");
		char line[256];
		if (fgets(line, sizeof(line), stdin) == NULL)
			continue;

		if (strcmp(line, "\n") == 0)
			continue;

		line[strlen(line) - 1] = '\0';

		char cmd[256];
		char line2[256];
		strncpy(line2, line, sizeof(line2) - 1);
		strncpy(cmd, strtok(line, " "), sizeof(cmd) - 1);

		if (!isAllowed(cmd))
		{
			printf("NOT ALLOWED!\n");
			continue;
		}

		if (strcmp(cmd, "sendmsg") == 0)
		{
			char *target = strtok(NULL, " ");
			char *msg = strtok(NULL, "\n");

			if (target == NULL)
			{
				printf("sendmsg: you have to specify target user\n");
				continue;
			}
			if (msg == NULL)
			{
				printf("sendmsg: you have to enter a message\n");
				continue;
			}
			sendmsg(uName, target, msg);
			continue;
		}

		if (strcmp(cmd, "exit") == 0)
			break;

		if (strcmp(cmd, "cd") == 0)
		{
			char *targetDir = strtok(NULL, " ");
			if (targetDir == NULL || strtok(NULL, " ") != NULL)
			{
				printf("-rsh: cd: too many or no arguments\n");
			}
			else
			{
				if (chdir(targetDir) != 0)
				{
					perror("Error changing directory");
				}
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

		char **cargv = NULL;
		char *path = NULL;

		path = malloc(strlen(cmd) + 1);
		if (path == NULL)
		{
			perror("Memory allocation failed for path");
			continue;
		}
		strcpy(path, cmd);

		cargv = malloc(sizeof(char *));
		if (cargv == NULL)
		{
			perror("Memory allocation failed for cargv");
			free(path);
			continue;
		}
		cargv[0] = malloc(strlen(cmd) + 1);
		strcpy(cargv[0], cmd);

		char *attrToken = strtok(line2, " ");
		attrToken = strtok(NULL, " ");
		int n = 1;

		while (attrToken != NULL)
		{
			n++;
			cargv = realloc(cargv, sizeof(char *) * n);
			if (cargv == NULL)
			{
				perror("Reallocation failed");
				break;
			}
			cargv[n - 1] = malloc(strlen(attrToken) + 1);
			strcpy(cargv[n - 1], attrToken);
			attrToken = strtok(NULL, " ");
		}

		cargv = realloc(cargv, sizeof(char *) * (n + 1));
		if (cargv == NULL)
		{
			perror("Reallocation failed for cargv");
			continue;
		}
		cargv[n] = NULL;

		posix_spawnattr_t attr;
		posix_spawnattr_init(&attr);

		pid_t pid;
		if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0)
		{
			perror("Error spawning process");
		}

		if (waitpid(pid, NULL, 0) == -1)
		{
			perror("Error waiting for process");
		}

		posix_spawnattr_destroy(&attr);

		for (int i = 0; i < n; i++)
		{
			free(cargv[i]);
		}
		free(cargv);
		free(path);
	}

	pthread_cancel(listenerThread);
	pthread_join(listenerThread, NULL);

	return 0;
}
