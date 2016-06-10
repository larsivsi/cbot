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

pcre *regcomp(const char *pattern)
{
	const char *pcre_err;
	int pcre_err_offset;
	pcre *ret = pcre_compile(pattern, PCRE_CASELESS | PCRE_UTF8 | PCRE_NO_UTF8_CHECK, &pcre_err, &pcre_err_offset, 0);
	if (ret == 0) {
		fprintf(stderr, "error while compiling pattern '%s', at %d: %s\n", pattern, pcre_err_offset, pcre_err);
		exit(1);
	}
	return ret;
}

void compile_patterns(struct patterns *patterns)
{
	// Privmsg
	patterns->privmsg = regcomp(":([^!]+)!(([^@]+)@(\\S+))\\sPRIVMSG\\s(\\S+)\\s:([^\\b]+)");
	// Kicks
	patterns->kick = regcomp(":([^!]+)!([^@]+)@(\\S+)\\sKICK\\s(\\S+)\\s(\\S+)\\s:");
	// Joins
	// alt: ":([^!]+)!([^@]+@\\S+)\\sJOIN\\s(\\S+)\\s(\\S+)\\s:(\\S+)";
	patterns->join = regcomp(":([^!]+)!(\\S+)\\sJOIN\\s:(\\S+)");
	// Urls
	// alt: "\\b((?:(?:([a-z][\\w\\.-]+:/{1,3})|www\\d{0,3}[.]|[a-z0-9.\\-]+[.][a-z]{2,4}/)(?:[^\\s()<>]+|\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\))+(?:\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\)|\\}\\]|[^\\s`!()\\[\\]{};:'\".,<>?])|[a-z0-9.\\-+_]+@[a-z0-9.\\-]+[.][a-z]{1,5}[^\\s/`!()\\[\\]{};:'\".,<>?]))";
	patterns->url = regcomp("(https?:\\/\\/\\S+)");
	// HTML page titles
	// alt: "<title>(.+)<\\/title>";
	patterns->html_title = regcomp("<title>([^<]+)<\\/title>");
	// Eightball
	patterns->command_eightball = regcomp("!8ball ([^$]+)");
	// Uptime
	patterns->command_uptime = regcomp("!uptime");
	// Give operator status
	patterns->command_op = regcomp("!op (\\S+)");
	// Say command for the console
	patterns->command_say = regcomp("say (\\S+) (.*)$");
	// kick command for the console
	patterns->command_kick = regcomp("kick (\\S+) (.+)$");

	// Timer
	patterns->command_timer = regcomp("!timer (.+)");
	// time patterns
	// {1w,1d,1h,1m,1s} message
	patterns->time_offset = regcomp("^(\\d{1,4}) ?(w|d|h|m|s) (\\S+)");
	// hh:mm message
	patterns->time_hourminute = regcomp("^(\\d\\d?):(\\d\\d?) (\\S+)");
	// dd/mm-hh:mm message
	patterns->time_timedate = regcomp("^(\\d\\d?)/(\\d\\d?)-(\\d\\d?):(\\d\\d?) (\\S+)");
	// day-hh:mm message
	patterns->time_daytime = regcomp("^(monday|tuesday|wednesday|thursday|friday|saturday|sunday|mon|tue|wed|thu|fri|sat|sun)-(\\d\\d?):(\\d\\d?) (\\S+)");
}

void free_patterns(struct patterns *patterns)
{
	pcre_free(patterns->privmsg);
	pcre_free(patterns->kick);
	pcre_free(patterns->join);
	pcre_free(patterns->url);
	pcre_free(patterns->html_title);
	pcre_free(patterns->command_eightball);
	pcre_free(patterns->command_uptime);
	pcre_free(patterns->command_say);
	pcre_free(patterns->command_kick);
	pcre_free(patterns->command_op);
	pcre_free(patterns->command_timer);
	pcre_free(patterns->time_offset);
	pcre_free(patterns->time_hourminute);
	pcre_free(patterns->time_timedate);
	pcre_free(patterns->time_daytime);
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
		char buffer[strlen("JOIN \n") + strlen(config->channels[i]) + 1]; // + 1 for terminating null byte
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
	char buffer[strlen("USER  host realmname :\nNICK \n") + strlen(config->user) + strlen(config->nick) + strlen(config->nick) + 1]; // + 1 for terminating null byte
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

	irc_init();

	if (socket_fd == -1) {
		printf(" - Connecting to %s:%s with nick %s, joining channels...\n",
			config->host, config->port, config->nick);
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


	int recv_size;
	char buffer[BUFFER_SIZE];
	char input[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);
	size_t buffer_length = 0;
	while (1) {
		int ret = select(socket_fd+1, &socket_set, 0, 0, 0);
		if (ret == -1) {
			printf(" >> Disconnected, reconnecting...\n");
			close(socket_fd);
			net_connect();
		}
		if (FD_ISSET(STDIN_FILENO, &socket_set)) {
			if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
				printf(" >> Error while reading from stdin!\n");
				continue;
			}

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
			} else if (strncmp(input, "kick ", 5) == 0) {
				int offsets[30];
				int offsetcount = pcre_exec(patterns->command_kick, 0, input, strlen(input), 0, 0, offsets, 30);
				if (offsetcount > 0) {
					char channel[BUFFER_SIZE];
					char user[BUFFER_SIZE];
					pcre_copy_substring(input, offsets, offsetcount, 1, channel, BUFFER_SIZE);
					pcre_copy_substring(input, offsets, offsetcount, 2, user, BUFFER_SIZE);
					char sendbuf[strlen("KICK   :Gene police! You! Out of the pool, now!\n") + strlen(channel) + strlen(user)];
					sprintf(sendbuf, "KICK %s %s :Gene police! You! Out of the pool, now!\n", channel, user);
					irc_send_str(sendbuf);
				}
			} else {
				printf(" >> Unrecognized command. Try 'quit'\n");
			}
			FD_SET(socket_fd, &socket_set);
		} else {
			if (buffer_length >= BUFFER_SIZE - 1) {
				printf(" >> what the fuck, IRCd, a line longer than 4k? dropping some buffer\n");
				memset(buffer, 0, BUFFER_SIZE);
				buffer_length = 0;
				continue;
			}

			recv_size = recv(socket_fd, buffer + buffer_length, BUFFER_SIZE - buffer_length - 1, 0);
			buffer[buffer_length] = '\0';
			buffer_length += recv_size;
			if (recv_size == 0) {
				printf(" >> recv_size is 0, assuming closed remote socket, reconnecting\n");
				close(socket_fd);
				printf("closed\n");
				net_connect();
				printf("reconnected\n");
			}
			char *newlinepos = 0;
			char *bufbegin = buffer;
			while ((newlinepos = strchr(bufbegin, '\n'))) {
				*newlinepos = 0;
				printf(" ~ %s\n", bufbegin);
				// Only handle privmsg
				if (irc_parse_input(bufbegin, irc, patterns)) {
					irc_handle_input(irc, patterns);
				}
				bufbegin = newlinepos + 1;
			}
			size_t bytes_removed = bufbegin - buffer;
			memmove(buffer, bufbegin, buffer_length - bytes_removed);
			buffer_length -= bytes_removed;
			memset(buffer + buffer_length, 0, BUFFER_SIZE - buffer_length);

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
