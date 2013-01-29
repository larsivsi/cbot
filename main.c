#include "common.h"

#include "config.h"
#include "eightball.h"
#include "log.h"
#include "title.h"
#include "timer.h"
#include "irc.h"

#include <curl/curl.h>
#include <pcre.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

struct patterns *patterns;
struct timeval t_begin, t_now;
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

	// Uptime
	pattern = "!uptime";
	if ((patterns->uptime = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile uptime", 0);
}

void free_patterns(struct patterns *patterns)
{
	pcre_free(patterns->privmsg);
	pcre_free(patterns->kick);
	pcre_free(patterns->url);
	pcre_free(patterns->html_title);
	pcre_free(patterns->eightball);
	pcre_free(patterns->timer);
	pcre_free(patterns->uptime);
}

void die(const char *msg, const char *error)
{
	fprintf(stderr, "%s: %s\n", msg, error);
	exit(1);
}

void tic()
{
	gettimeofday(&t_begin, 0);
}

// Returns uptime in usecs
long toc()
{
	gettimeofday(&t_now, 0);
	return (t_now.tv_sec-t_begin.tv_sec)*1000000 + t_now.tv_usec-t_begin.tv_usec;
}

void uptime_in_stf(long secs, char *buf)
{
	long mins = 0, hours = 0, days = 0;

	if (secs >= 60)
	{
		mins = secs / 60;
		secs = secs % 60;
	}
	if (mins >= 60)
	{
		hours = mins / 60;
		mins = mins % 60;
	}
	if (hours >= 24)
	{
		days = hours / 24;;
		hours = hours % 24;
	}

	if (days > 0)
		sprintf(buf, "%ldd %ldh %ldm %lds", days, hours, mins, secs);
	else if (hours > 0)
		sprintf(buf, "%ldh %ldm %lds", hours, mins, secs);
	else if (mins > 0)
		sprintf(buf, "%ldm %lds", mins, secs);
	else
		sprintf(buf, "%lds", secs);
}

void handle_input(struct recv_data *in, struct patterns *patterns)
{
	log_message(in);

	const char *msg = in->message;
	int offsets[30];	
	// Check URLs
	int offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), 0, 0, offsets, 30);
	while (offsets[1] < strlen(msg) && offsetcount > 0) {
		char url[BUFFER_SIZE];
		pcre_copy_substring(msg, offsets, offsetcount, 1, url, BUFFER_SIZE);
		get_title_from_url(in, url);
		log_url(in, url);
		offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), offsets[1], 0, offsets, 30);
	}

	// 8ball
	offsetcount = pcre_exec(patterns->eightball, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		char arguments[BUFFER_SIZE];
		pcre_copy_substring(msg, offsets, offsetcount, 1, arguments, BUFFER_SIZE);
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

	// Uptime
	offsetcount = pcre_exec(patterns->uptime, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		long elapsed_secs = toc() / 1000000;
		char *buf = (char*)malloc(BUFFER_SIZE);
		char *stf = (char*)malloc(BUFFER_SIZE);
		uptime_in_stf(elapsed_secs, stf);
		sprintf(buf, "PRIVMSG %s :I have been operational for %ld seconds, aka. %s\n",
			in->channel, elapsed_secs, stf);
		send_str(buf);
		free(buf);
		free(stf);
	}

}

int main(int argc, char **argv)
{
	tic();
	config = malloc(sizeof(*config));
	if (argc == 1) {
		load_config("cbot.conf");
	} else if (argc == 2) {
		load_config(argv[1]);
	} else {
		printf("Usage: %s [configfile]\n", argv[0]);
		exit(0);
	}
	printf(" - Connecting to %s:%s with nick %s, joining channel %s...\n",
			config->host, config->port, config->nick, config->channels);

	// Set up cURL
	curl_global_init(CURL_GLOBAL_ALL);

	// Set up db connection for logging
	log_init();

	int err, recv_size;
	char buffer[BUFFER_SIZE];
	char input[BUFFER_SIZE];
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
	sprintf(buffer, "USER %s host realmname :%s\nNICK %s\nJOIN %s\n",
		config->user, config->nick, config->nick, config->channels);
	send_str(buffer);

	struct recv_data *irc = malloc(sizeof(struct recv_data));
	patterns = malloc(sizeof(*patterns));
	compile_patterns(patterns);

	while (select(socket_fd+1, &socket_set, 0, 0, 0) != -1) {
		if (FD_ISSET(STDIN_FILENO, &socket_set)) {
			fgets(input, BUFFER_SIZE, stdin);
			if (strcmp(input, "quit\n") == 0) {
				printf(">> Bye!\n");
				break;
			} else {
				printf(">> Unrecognized command. Try 'quit'\n");
			}
			FD_SET(socket_fd, &socket_set);
		}
		else {
			recv_size = recv(socket_fd, buffer, BUFFER_SIZE-1, 0);
			if (recv_size == 0) {
				printf(">> recv_size is 0, assuming closed remote socket!");
				break;
			}
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

	log_terminate();
	irc_terminate();
	free_config();

	curl_global_cleanup();

	return 0;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
