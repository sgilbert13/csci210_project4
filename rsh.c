#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

struct message {
	char source[50];
	char target[50]; 
	char msg[200];
};

void terminate(int sig) {
        printf("Exiting....\n");
        fflush(stdout);
        exit(0);
}

void sendmsg (char *user, char *target, char *msg) {
	// TODO:
	// Send a request to the server to send the message (msg) to the target user (target)
	// by creating the message structure and writing it to server's FIFO
	struct message message = {0}; // Initialize structure with zeros
snprintf(message.source, sizeof(message.source), "%s", user);
snprintf(message.target, sizeof(message.target), "%s", target);
snprintf(message.msg, sizeof(message.msg), "%s", msg);

int server = open("serverFIFO", O_WRONLY);
if (server == -1) {
    perror("Error opening serverFIFO");
    exit(EXIT_FAILURE);
}

if (write(server, &message, sizeof(message)) == -1) {
    perror("Error writing to serverFIFO");
    close(server);
    exit(EXIT_FAILURE);
}

close(server);
}

void* messageListener(void *arg) {
	// TODO:
	// Read user's own FIFO in an infinite loop for incoming messages
	// The logic is similar to a server listening to requests
	// print the incoming message to the standard output in the
	// following format
	// Incoming message from [source]: [message]
	// put an end of line at the end of the message
	int user = open(uName, O_RDONLY);
if (user == -1) {
    perror("Error opening user FIFO");
    exit(EXIT_FAILURE);
}

struct message msg;
while (1) {
    ssize_t bytesRead = read(user, &msg, sizeof(msg));

    if (bytesRead == -1) {
        perror("Error reading from user FIFO");
        close(user);
        exit(EXIT_FAILURE);
    } else if (bytesRead == 0) {
        // End of file (EOF) or no data available
        continue;
    }

    // Ensure only fully read messages are processed
    if (bytesRead == sizeof(msg)) {
        printf("Incoming message from %s: %s\n", msg.source, msg.msg);
    } else {
        fprintf(stderr, "Partial message read, ignoring...\n");
    }
}

// Cleanup (though unreachable in this case since the loop is infinite)
close(user);


	pthread_exit((void*)0);
}

int isAllowed(const char*cmd) {
	int i;
	for (i=0;i<N;i++) {
		if (strcmp(cmd,allowed[i])==0) {
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv; 
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc!=2) {
	printf("Usage: ./rsh <username>\n");
	exit(1);
    }
    signal(SIGINT,terminate);

    strcpy(uName,argv[1]);

    // TODO:
    // create the message listener thread
	pthread_t listener_thread;
pthread_attr_t attr;

// Initialize thread attributes
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

// Create the detached thread
if (pthread_create(&listener_thread, &attr, messageListener, NULL) != 0) {
    perror("Failed to create listener thread");
    exit(EXIT_FAILURE);
}

// Destroy thread attributes object
pthread_attr_destroy(&attr);

    while (1) {

	fprintf(stderr,"rsh>");

	if (fgets(line,256,stdin)==NULL) continue;

	if (strcmp(line,"\n")==0) continue;

	line[strlen(line)-1]='\0';

	char cmd[256];
	char line2[256];
	strcpy(line2,line);
	strcpy(cmd,strtok(line," "));

	if (!isAllowed(cmd)) {
		printf("NOT ALLOWED!\n");
		continue;
	}

	if (strcmp(cmd,"sendmsg")==0) {
		// TODO: Create the target user and
		// the message string and call the sendmsg function

		// NOTE: The message itself can contain spaces
		// If the user types: "sendmsg user1 hello there"
		// target should be "user1" 
		// and the message should be "hello there"

		// if no argument is specified, you should print the following
		// printf("sendmsg: you have to specify target user\n");
		// if no message is specified, you should print the followingA
 		// printf("sendmsg: you have to enter a message\n");
		// Get the target user
char *token = strtok(NULL, " ");
if (token == NULL) {
    printf("sendmsg: you have to specify target user\n");
    continue;
}

// Copy the user name to a local variable
char user[20] = "";  // Fixed-size buffer
strncpy(user, token, sizeof(user) - 1);  // Ensure null termination

// Get the message
token = strtok(NULL, " ");
if (token == NULL) {
    printf("sendmsg: you have to enter a message\n");
    continue;
}

// Construct the full message
char message[256] = "";  // Fixed-size buffer
strncpy(message, token, sizeof(message) - 1);
token = strtok(NULL, " ");
while (token != NULL) {
    // Check if appending this token will overflow the buffer
    if (strlen(message) + strlen(token) + 2 >= sizeof(message)) {
        printf("sendmsg: message too long\n");
        break;
    }
    strcat(message, " ");
    strcat(message, token);
    token = strtok(NULL, " ");
}

// Send the message
sendmsg(uName, user, message);


		continue;
	}

	if (strcmp(cmd,"exit")==0) break;

	if (strcmp(cmd,"cd")==0) {
		char *targetDir=strtok(NULL," ");
		if (strtok(NULL," ")!=NULL) {
			printf("-rsh: cd: too many arguments\n");
		}
		else {
			chdir(targetDir);
		}
		continue;
	}

	if (strcmp(cmd,"help")==0) {
		printf("The allowed commands are:\n");
		for (int i=0;i<N;i++) {
			printf("%d: %s\n",i+1,allowed[i]);
		}
		continue;
	}

	cargv = (char**)malloc(sizeof(char*));
	cargv[0] = (char *)malloc(strlen(cmd)+1);
	path = (char *)malloc(9+strlen(cmd)+1);
	strcpy(path,cmd);
	strcpy(cargv[0],cmd);

	char *attrToken = strtok(line2," "); /* skip cargv[0] which is completed already */
	attrToken = strtok(NULL, " ");
	int n = 1;
	while (attrToken!=NULL) {
		n++;
		cargv = (char**)realloc(cargv,sizeof(char*)*n);
		cargv[n-1] = (char *)malloc(strlen(attrToken)+1);
		strcpy(cargv[n-1],attrToken);
		attrToken = strtok(NULL, " ");
	}
	cargv = (char**)realloc(cargv,sizeof(char*)*(n+1));
	cargv[n] = NULL;

	// Initialize spawn attributes
	posix_spawnattr_init(&attr);

	// Spawn a new process
	if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
		perror("spawn failed");
		exit(EXIT_FAILURE);
	}

	// Wait for the spawned process to terminate
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid failed");
		exit(EXIT_FAILURE);
	}

	// Destroy spawn attributes
	posix_spawnattr_destroy(&attr);

    }
    return 0;
}

