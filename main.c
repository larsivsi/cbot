#include "common.h"

#include "config.h"
#include "eightball.h"
#include "log.h"
#include "web.h"
#include "timer.h"
#include "irc.h"
#include "markov.h"

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
#include <curl/curl.h>

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

	// Kicks
	pattern = ":([^!]+)!([^@]+)@(\\S+)\\sKICK\\s(\\S+)\\s(\\S+)\\s:";
	if ((patterns->kick = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == 0)
		die("pcre compile kick", 0);

	// Joins
	//pattern = ":([^!]+)!([^@]+@\\S+)\\sJOIN\\s(\\S+)\\s(\\S+)\\s:(\\S+)";
	pattern = ":([^!]+)!(\\S+)\\sJOIN\\s:(\\S+)";
	if ((patterns->join = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == 0)
		die("pcre compile join", 0);

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
	if ((patterns->command_eightball = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile 8ball", 0);

	// Timer
	pattern = "!timer (\\d{1,4})";
	if ((patterns->command_timer = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile timer", 0);

	// Uptime
	pattern = "!uptime";
	if ((patterns->command_uptime = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile uptime", 0);

	// Say command for the console
	pattern = "say (\\S+) (.*)$";
	if ((patterns->command_say = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile command_say", 0);

	// Twitter
	pattern = "!twitter (\\S+)";
	if ((patterns->command_twitter = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile twitter", 0);

	// Tweet in HTML
	pattern = "<p class=\"js-tweet-text tweet-text\">(.+)</p>";
	if ((patterns->tweet = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL)
		die("pcre compile tweet", 0);
}

void free_patterns(struct patterns *patterns)
{
	pcre_free(patterns->privmsg);
	pcre_free(patterns->kick);
	pcre_free(patterns->join);
	pcre_free(patterns->url);
	pcre_free(patterns->html_title);
	pcre_free(patterns->tweet);
	pcre_free(patterns->command_eightball);
	pcre_free(patterns->command_timer);
	pcre_free(patterns->command_uptime);
	pcre_free(patterns->command_say);
	pcre_free(patterns->command_twitter);
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

void print_usage()
{
	printf("Usage: cbot [-c configfile]\n");
	exit(0);
}

void terminate()
{

	log_terminate();
	irc_terminate();
	markov_terminate();
	free_config();

	curl_global_cleanup();
}

void join_channels()
{
	int i=0;
	while (config->channels[i]) {
		char buffer[strlen("JOIN \n") + strlen(config->channels[i])];
		sprintf(buffer, "JOIN %s\n", config->channels[i++]);
		irc_send_str(buffer);
	}
}

void net_connect()
{
	int err;
	// Connect

	struct addrinfo hints;
	struct addrinfo *srv;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if ((err = getaddrinfo(config->host, config->port, &hints, &srv)) != 0)
		die("getaddrinfo", gai_strerror(err));
	if ((socket_fd = socket(srv->ai_family, srv->ai_socktype, 0)) < 0)
		die("socket", gai_strerror(socket_fd));
	if ((err = connect(socket_fd, srv->ai_addr, srv->ai_addrlen)) != 0)
		die("connect", gai_strerror(err));
	freeaddrinfo(srv);

	// Join
	printf("connecting...\n");
	char buffer[strlen("USER  host realmname :\nNICK \n") + strlen(config->user) + strlen(config->nick) + strlen(config->nick)];
	sprintf(buffer, "USER %s host realmname :%s\nNICK %s\n",
			config->user, config->nick, config->nick);
	irc_send_str(buffer);

	join_channels();
}

int main(int argc, char **argv)
{
	tic();

	char *conf_file = NULL;
	socket_fd = -1;
	for (int i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-c")) {
			if (argc <= i) {
				print_usage();
			}
			conf_file = argv[++i];
		} else if (!strcmp(argv[i], "-fd")) {
			if (argc <= i) {
				print_usage();
			}
			socket_fd = atoi(argv[++i]);
		} else {
			printf(" >> unknown option: %s\n", argv[i]);
		}
	}

	if (!conf_file)
		conf_file = "cbot.conf";
	load_config(conf_file);

	char channels[BUFFER_SIZE];
	for (int i = 0; config->channels[i] != NULL; i++) {
		if (i > 0) {
			strcat(channels, ",");
			strcat(channels, config->channels[i]);
		} else {
			strcpy(channels, config->channels[i]);
		}
	}
	// Set rand seed
	srand(time(NULL));

	// Set up cURL
	curl_global_init(CURL_GLOBAL_ALL);

	// Set up db connection for logging
	if (config->enabled_modules & MODULE_LOG) {
		log_init();
	}

	// Parse markov corpus
	if (config->enabled_modules & MODULE_MARKOV) {
		markov_init(config->markovcorpus);
	}

	int recv_size;
	char buffer[BUFFER_SIZE];
	char input[BUFFER_SIZE];

	irc_init();

	if (socket_fd == -1) {
		printf(" - Connecting to %s:%s with nick %s, joining channels %s...\n",
			config->host, config->port, config->nick, channels);
		close(socket_fd);
		net_connect();
	} else { // In-place upgrade yo
		printf(" >> Already connected, upgraded in-place!\n");
		join_channels();
	}

	struct recv_data *irc = malloc(sizeof(struct recv_data));
	patterns = malloc(sizeof(*patterns));
	compile_patterns(patterns);

	// Select param
	fd_set socket_set;
	FD_ZERO(&socket_set);
	FD_SET(STDIN_FILENO, &socket_set);
	FD_SET(socket_fd, &socket_set);

	while (1) {
		int ret = select(socket_fd+1, &socket_set, 0, 0, 0);
		if (ret == -1) {
			printf(" >> Disconnected, reconnecting...\n");
			close(socket_fd);
			net_connect();
		}
		if (FD_ISSET(STDIN_FILENO, &socket_set)) {
			fgets(input, BUFFER_SIZE, stdin);
			if (strcmp(input, "quit\n") == 0) {
				printf(" >> Bye!\n");
				break;
			} else if (strcmp(input, "reload\n") == 0) {
				terminate();
				free(irc);
				free_patterns(patterns);
				free(patterns);

				// Set up arguments
				char * arguments[6];
				arguments[0] = argv[0];
				arguments[1] = "-c";
				arguments[2] = conf_file;
				arguments[3] = "-fd";
				char fdstring[snprintf(NULL, 0, "%d", socket_fd)];
				sprintf(fdstring, "%d", socket_fd);
				arguments[4] = fdstring;
				arguments[5] = NULL;

				printf(" >> Upgrading...\n");
				execvp(argv[0], arguments);

				printf(" !!! Execvp failing, giving up...\n");
				exit(-1);
			} else if (strcmp(input, "upgrade\n") == 0) {
				system("(git pull --ff-only && make) &");
				printf(" >> Upgrading...\n");
			} else if (strncmp(input, "say ", 4) == 0) {
				int offsets[30];
				int offsetcount = pcre_exec(patterns->command_say, 0, input, strlen(input), 0, 0, offsets, 30);
				if (offsetcount > 0) {
					char channel[BUFFER_SIZE];
					char message[BUFFER_SIZE];
					pcre_copy_substring(input, offsets, offsetcount, 1, channel, BUFFER_SIZE);
					pcre_copy_substring(input, offsets, offsetcount, 2, message, BUFFER_SIZE);
					char sendbuf[strlen("PRIVMSG  : ") + strlen(channel) + strlen(message)];
					sprintf(sendbuf, "PRIVMSG %s :%s\n", channel, message);
					irc_send_str(sendbuf);
				}
			} else {
				printf(" >> Unrecognized command. Try 'quit'\n");
			}
			FD_SET(socket_fd, &socket_set);
		}
		else {
			recv_size = recv(socket_fd, buffer, BUFFER_SIZE-1, 0);
			if (recv_size == 0) {
				printf(" >> recv_size is 0, assuming closed remote socket, reconnecting\n");
				close(socket_fd);
				printf("closed\n");
				net_connect();
				printf("reconnected\n");
			}
			// Add \0 to terminate string
			buffer[recv_size] = '\0';
			//printf("%s", buffer);
			// Only handle privmsg
			if (irc_parse_input(buffer, irc, patterns))
				irc_handle_input(irc, patterns);
			FD_SET(STDIN_FILENO, &socket_set);
		}
	}
	printf(" >> Socket closed, quitting...\n");

	close(socket_fd);

	free(irc);
	free_patterns(patterns);
	free(patterns);

	terminate();

	return 0;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
