#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFFER 512

// structs
struct recv_data {
	char nick[32];
	char user[32];
	char server[64];
	char channel[32];
	char message[BUFFER];
	int is_ready;
};

// prototypes
void die(char* msg);
void handle_input(struct recv_data in);

void die(char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

void handle_input(struct recv_data in)
{
	printf("%s\n",in.message);
}

int main(int argc, char **argv)
{
	if (argc != 6) {
		printf("Usage: %s nick user server port channel\n", argv[0]);
		exit(0);
	}
	const char *nick = argv[1];
	const char *user = argv[2];
	const char *server = argv[3];
	const int port = atoi(argv[4]);
	const char *channel = argv[5];

	int socket_id;
	if ((socket_id = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		die("socket could not be opened");
	struct recv_data *irc = malloc(sizeof(*irc));
}
