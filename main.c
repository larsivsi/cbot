#include "main.h"

#include "title.h"
#include "config.h"
#include "log.h"
#include "eightball.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <curl/curl.h>

/// Global variables, used by multiple threads
int socket_fd;
char *send_buffer = 0;
size_t send_buffer_size = 0;
size_t send_buffer_used = 0;
pthread_mutex_t *send_mutex = 0;
pthread_cond_t *send_data_ready = 0;
pthread_t *send_thread = 0;
int send_thread_running = 0;

struct patterns *patterns;

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

// Returns 1 on privmsg, 0 otherwise
int parse_input(char *msg, struct recv_data *in, struct patterns *patterns)
{
	if (strncmp(msg, "PING :", 6) == 0) {
		// Turn the ping into a pong :D
		msg[1] = 'O';
		send_str(msg);
		return 0;
	}
	//TODO: check 30
	int offsets[30];
	int offsetcount = pcre_exec(patterns->privmsg, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount == 6) {
		pcre_copy_substring(msg, offsets, offsetcount, 1, in->nick, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 2, in->user, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 3, in->server, 64);
		pcre_copy_substring(msg, offsets, offsetcount, 4, in->channel, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 5, in->message, BUFFER);
		// In case of privmsgs
		if (strcmp(in->channel, config->nick) == 0)
			strcpy(in->channel, in->nick);
		return 1;
	}
	offsetcount = pcre_exec(patterns->kick, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount == 6) {
		pcre_copy_substring(msg, offsets, offsetcount, 1, in->nick, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 2, in->user, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 3, in->server, 64);
		pcre_copy_substring(msg, offsets, offsetcount, 4, in->channel, 32);
		// User that got kicked
		pcre_copy_substring(msg, offsets, offsetcount, 5, in->message, 32);
		if (strcmp(in->message, config->nick) == 0) {
			printf("Got kicked, rejoining\n");
			char rejoin[40];
			sprintf(rejoin, "JOIN #%s\n", config->channel);
			send_str(rejoin);
		}
		return 0;
	}
	return 0;
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

void send_str(char *msg)
{
	pthread_mutex_lock(send_mutex);

	int length = strlen(msg);
	// Check if we have enough space
	if (length > send_buffer_size - send_buffer_used) {
		int new_buffer_size = send_buffer_size + BUFFER;
		if (!realloc(send_buffer, new_buffer_size)) {
			die("Unable to allocate more memory for send buffer", strerror(errno));
		}
		send_buffer_size = new_buffer_size;
	}

	memcpy(&send_buffer[send_buffer_used], msg, length);
	send_buffer_used += length;

	pthread_cond_signal(send_data_ready);

	pthread_mutex_unlock(send_mutex);

	// Print out what we have done
	printf("--> %s", msg);
}

void *send_loop(void *arg)
{
	// To make sure the send thread reaches wait() before the first signal()
	pthread_mutex_unlock(send_mutex);
	pthread_mutex_lock(send_mutex);
	while (send_thread_running) {
		pthread_cond_wait(send_data_ready, send_mutex);
		while (send_buffer_used > 0) {
			int sent = send(socket_fd, send_buffer, send_buffer_used, 0);
			if (sent == -1) {
				die("Unable to send", strerror(errno));
			}
			send_buffer_used -= sent;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	config = malloc(sizeof(*config));
	if (argc == 1) {
		load_config();
	}
	else {
		printf("Usage: %s\n", argv[0]);
		exit(0);
	}
	printf("nick: %s, user: %s, host: %s, port: %s, channel: %s\n",
		config->nick, config->user, config->host, config->port, config->channel);

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

	// Allocate the send buffer
	send_buffer = malloc(BUFFER);
	send_buffer_size = BUFFER;

	// Create our mutexes
	send_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	send_data_ready = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	pthread_mutex_init(send_mutex, 0);
	pthread_cond_init(send_data_ready, 0);

	// Create our sending thread
	send_thread = (pthread_t*)malloc(sizeof(pthread_t));
	send_thread_running = 1;
	pthread_create(send_thread, 0, &send_loop, 0);
	//TODO: Find a better way of making send_thread reach wait() before the first signal()
	pthread_mutex_lock(send_mutex);

	// Select param
	fd_set socket_set;
	FD_ZERO(&socket_set);
	FD_SET(STDIN_FILENO, &socket_set);
	FD_SET(socket_fd, &socket_set);

	// Set rand seed
	srand(time(NULL));

	// Join
	sprintf(buffer, "USER %s host realmname :%s\nNICK %s\nJOIN #%s\n",
		config->user, config->nick, config->nick, config->channel);
	send_str(buffer);

	struct recv_data *irc = malloc(sizeof(*irc));
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

	send_thread_running = 0;
	pthread_mutex_unlock(send_mutex);
	pthread_join(*send_thread, 0);
	free(send_thread);

	pthread_mutex_destroy(send_mutex);
	free(send_mutex);
	pthread_cond_destroy(send_data_ready);
	free(send_data_ready);

	close(socket_fd);
	curl_global_cleanup();
	free(irc);
	free(send_buffer);
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

	curl_global_cleanup();

	return 0;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
