// for getaddrinfo and the likes
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

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
void die(char *msg, int err_code);
void handle_input(struct recv_data in);
int send_str(int socket_id, char *msg);

void die(char *msg, int err_code)
{
	fprintf(stderr, "%s: %s\n", msg, gai_strerror(err_code));
	exit(1);
}

void handle_input(struct recv_data in)
{
	printf("%s\n",in.message);
}

int send_str(int socket_id, char *msg)
{
	char send_str[BUFFER];
	sprintf(send_str, "%s\n", msg);
	return send(socket_id, send_str, strlen(send_str), 0);
}

int main(int argc, char **argv)
{
	if (argc != 6) {
		printf("Usage: %s nick user host port channel\n", argv[0]);
		exit(0);
	}
	const char *nick = argv[1];
	const char *user = argv[2];
	const char *host = argv[3];
	const char *port = argv[4];
	const char *channel = argv[5];

	int socket_id, err, recv_size;
	char buffer[BUFFER];
	struct addrinfo hints;
	struct addrinfo *srv;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	if ((err = getaddrinfo(host, port, &hints, &srv)) != 0)
		die("getaddrinfo", err);
	if ((socket_id = socket(srv->ai_family, srv->ai_socktype, 0)) < 0)
		die("socket", socket_id);
	if ((err = connect(socket_id, srv->ai_addr, srv->ai_addrlen)) != 0)
		die("connect", err);

	sprintf(buffer, "USER %s host realmname :%s\nNICK %s\nJOIN #%s", user, nick, nick, channel);
	if ((err = send_str(socket_id, buffer)) < 0)
		die("send user data", err);

	struct recv_data *irc = malloc(sizeof(*irc));
	
	while ((recv_size = recv(socket_id, buffer, BUFFER, 0)) >= 0) {
		buffer[recv_size] = '\0';
		printf("%s", buffer);
	}

	close(socket_id);
	free(irc);
	return 0;
}
