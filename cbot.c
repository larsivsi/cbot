// for getaddrinfo and the likes
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pcre.h>

#define BUFFER 512

// structs
struct recv_data {
	char nick[32];
	char user[32];
	char server[64];
	char channel[32];
	char message[BUFFER];
};

struct patterns {
	pcre *privmsg;
	pcre *kick;
    pcre *ping;
};

// prototypes
void compile_patterns(struct patterns *patterns);
void die(char *msg, int err_code);
void parse_input(char *msg, struct recv_data *in, struct patterns *patterns);
int send_str(int socket_id, char *msg);

void compile_patterns(struct patterns *patterns)
{
	const char *pcre_err;
	int pcre_err_off;
	char *pattern = ":([^!]+)!([^@]+)@(\\S+)\\sPRIVMSG\\s(\\S+)\\s:([^\\b]+)";
	if ((patterns->privmsg = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile privmsg", 0);
    pattern = "PING :(\\S+)";
    if ((patterns->ping = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile ping", 0);
}

void die(char *msg, int err_code)
{
	fprintf(stderr, "%s: %s\n", msg, gai_strerror(err_code));
	exit(1);
}

void parse_input(char *msg, struct recv_data *in, struct patterns *patterns)
{
	//TODO: check 30
	int offsets[30];
	int offsetcount = 30;
	offsetcount = pcre_exec(patterns->privmsg, NULL, msg, strlen(msg), 0, 0, offsets, offsetcount);
	if (offsetcount > 0)
		printf("offsetcount: %d\n", offsetcount);
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
	struct patterns *patterns = malloc(sizeof(*patterns));
	compile_patterns(patterns);

	while ((recv_size = recv(socket_id, buffer, BUFFER, 0)) >= 0) {

        if (strncmp(buffer, "PING :", 6) == 0) {
            // turn the ping into a pong :D
            buffer[1] = 'O';
		    buffer[recv_size-1] = '\0';
            send_str(socket_id, buffer);
            printf(buffer);
        }
        else {
		    //overwrite \n with \0
		    buffer[recv_size-1] = '\0';
		    puts(buffer);
    		parse_input(buffer, irc, patterns);
        }
	}

	close(socket_id);
	free(irc);
	free(patterns);
	return 0;
}
