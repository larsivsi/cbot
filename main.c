#include "common.h"

#include "config.h"
#include "eightball.h"
#include "log.h"
#include "title.h"
#include "timer.h"

#include "irc.h"

#include <curl/curl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

struct patterns *patterns;
int socket_fd;

void compile_patterns(struct patterns *patterns)
{
	const char *pcre_err;
	int pcre_err_off;
	// Privmsg
	char *pattern = ":([^!]+)!([^@]+)@(\\S+)\\sPRIVMSG\\s(\\S+)\\s:([^\\b]+)";
	if ((patterns->privmsg = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == 0)
		die("pcre compile privmsg", 0);
	pattern = ":([^!]+)!([^@]+)@(\\S+)\\sKICK\\s(\\S+)\\s(\\S+)\\s:";
	// Kicks
	if ((patterns->kick = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == 0)
		die("pcre compile kick", 0);

	// Urls
	//    pattern = "\\b((?:(?:([a-z][\\w\\.-]+:/{1,3})|www\\d{0,3}[.]|[a-z0-9.\\-]+[.][a-z]{2,4}/)(?:[^\\s()<>]+|\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\))+(?:\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\)|\\}\\]|[^\\s`!()\\[\\]{};:'\".,<>?])|[a-z0-9.\\-+_]+@[a-z0-9.\\-]+[.][a-z]{1,5}[^\\s/`!()\\[\\]{};:'\".,<>?]))";
	pattern = "(http:\\/\\/\\S+)";
	if ((patterns->url = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile url", 0);

	// HTML page titles
	//    pattern = "<title>(.+)<\\/title>";
	pattern = "<title>([^<]+)<\\/title>";
	if ((patterns->html_title = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile title", 0);
	// Eightball
	pattern = "!8ball ([^$]+)";
	if ((patterns->eightball = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile 8ball", 0);
	// Timer
	pattern = "!timer (\\d{1,4})";
	if ((patterns->timer = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile timer", 0);
}

void free_patterns(struct patterns *patterns)
{
	pcre_free(patterns->privmsg);
	pcre_free(patterns->kick);
	pcre_free(patterns->url);
	pcre_free(patterns->html_title);
	pcre_free(patterns->eightball);
	pcre_free(patterns->timer);
}

void die(const char *msg, const char *error)
{
	fprintf(stderr, "%s: %s\n", msg, error);
	exit(1);
}

void handle_input(struct recv_data *in, struct patterns *patterns)
{
	log_message(in);

	const char *msg = in->message;
	int offsets[30];	
	// Check URLs
	int offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), 0, 0, offsets, 30);
	while (offsets[1] < strlen(msg) && offsetcount > 0) {
		char url[BUFFER];
		pcre_copy_substring(msg, offsets, offsetcount, 1, url, BUFFER);
		get_title_from_url(in, url);
		offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), offsets[1], 0, offsets, 30);
	}

	// 8ball
	offsetcount = pcre_exec(patterns->eightball, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		char arguments[BUFFER];
		pcre_copy_substring(msg, offsets, offsetcount, 1, arguments, BUFFER);
		eightball(in, arguments);
	}

	// Timer
	offsetcount = pcre_exec(patterns->timer, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		// We limit at 4 digits
		char time[4];
		pcre_copy_substring(msg, offsets, offsetcount, 1, time, 4);
		int seconds = atoi(time)*60;
		set_timer(in->nick, in->channel, seconds);
	}
}

int main(int argc, char **argv)
{
	config = malloc(sizeof(*config));
	if (argc == 1) {
		load_config("cbot.conf");
	} else if (argc == 2) {
		load_config(argv[1]);
	} else {
		printf("Usage: %s [configfile]\n", argv[0]);
		exit(0);
	}
	printf(" - Connecting to %s:%s with nick %s, joining channel #%s...\n",
			config->host, config->port, config->nick, config->channel);

	// Set up cURL
	curl_global_init(CURL_GLOBAL_ALL);

	// Set up db connection for logging
	log_init();

	int err, recv_size;
	char buffer[BUFFER];
	struct addrinfo hints;
	struct addrinfo *srv;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	// Connect
	if ((err = getaddrinfo(config->host, config->port, &hints, &srv)) != 0)
		die("getaddrinfo", gai_strerror(err));
	if ((socket_fd = socket(srv->ai_family, srv->ai_socktype, 0)) < 0)
		die("socket", gai_strerror(socket_fd));
	if ((err = connect(socket_fd, srv->ai_addr, srv->ai_addrlen)) != 0)
		die("connect", gai_strerror(err));
	freeaddrinfo(srv);

	// Select param
	fd_set socket_set;
	FD_ZERO(&socket_set);
	FD_SET(STDIN_FILENO, &socket_set);
	FD_SET(socket_fd, &socket_set);

	irc_init();

	// Set rand seed
	srand(time(NULL));

	// Join
	sprintf(buffer, "USER %s host realmname :%s\nNICK %s\nJOIN #%s\n",
		config->user, config->nick, config->nick, config->channel);
	send_str(buffer);

	struct recv_data *irc = malloc(sizeof(struct recv_data));
	patterns = malloc(sizeof(*patterns));
	compile_patterns(patterns);

	while (select(socket_fd+1, &socket_set, 0, 0, 0) != -1) {
		if (FD_ISSET(STDIN_FILENO, &socket_set)) {
			char input[BUFFER];
			fgets(input, BUFFER, stdin);
			if (strcmp(input, "quit\n") == 0) {
				printf(">> Bye!\n");
				break;
			} else {
				printf(">> Unrecognized command. Try 'quit'\n");
			}
			FD_SET(socket_fd, &socket_set);
		}
		else {
			recv_size = recv(socket_fd, buffer, BUFFER-1, 0);
			// Add \0 to terminate string
			buffer[recv_size] = '\0';
			printf("%s", buffer);	
			// Only handle privmsg
			if (parse_input(buffer, irc, patterns))
				handle_input(irc, patterns);
			FD_SET(STDIN_FILENO, &socket_set);
		}
	}

	close(socket_fd);
	free(irc);
	free_patterns(patterns);
	free(patterns);
	free(config->nick);
	free(config->user);
	free(config->host);
	free(config->port);
	free(config->channel);
	free(config->db_connection_string);
	free(config);

	log_terminate();
	irc_terminate();

	curl_global_cleanup();

	return 0;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
