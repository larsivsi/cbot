// For getaddrinfo and the likes
#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pcre.h>
#include <pthread.h>
#include <sys/select.h>
#include <curl/curl.h>

#define BUFFER 512
#define HTTP_BUFFER 1024

// Structs
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
	pcre *url;
	pcre *html_title;
};

/// Global variables, used by multiple threads
int socket_fd;
char *send_buffer = 0;
char http_buffer[HTTP_BUFFER];
size_t http_buffer_pos;
size_t send_buffer_size = 0;
size_t send_buffer_used = 0;
pthread_mutex_t *send_mutex = 0;
pthread_mutex_t *send_sleep_mutex = 0;
pthread_t *send_thread = 0;
int send_thread_running = 0;

// Config options
char *nick;
char *user;
char *host;
char *port;
char *channel;

// Prototypes
void compile_patterns(struct patterns *patterns);
void die(const char *msg, const char *err);
void parse_input(char *msg, struct recv_data *in, struct patterns *patterns);
void send_str(int socket_fd, char *msg);

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
}

void die(const char *msg, const char *error)
{
	fprintf(stderr, "%s: %s\n", msg, error);
	exit(1);
}

void parse_input(char *msg, struct recv_data *in, struct patterns *patterns)
{
	if (strncmp(msg, "PING :", 6) == 0) {
		// Turn the ping into a pong :D
	        msg[1] = 'O';
		send_str(socket_fd, msg);
		return;
	}
	//TODO: check 30
	int offsets[30];
	int offsetcount = 30;
	offsetcount = pcre_exec(patterns->privmsg, 0, msg, strlen(msg), 0, 0, offsets, offsetcount);
	if (offsetcount == 6) {
		pcre_copy_substring(msg, offsets, offsetcount, 1, in->nick, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 2, in->user, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 3, in->server, 64);
		pcre_copy_substring(msg, offsets, offsetcount, 4, in->channel, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 5, in->message, BUFFER);
		// In case of privmsgs
		if (strcmp(in->channel, nick) == 0)
			strcpy(in->channel, in->nick);
		return;
	}
	offsetcount = 30;
	offsetcount = pcre_exec(patterns->kick, 0, msg, strlen(msg), 0, 0, offsets, offsetcount);
	if (offsetcount == 6) {
		pcre_copy_substring(msg, offsets, offsetcount, 1, in->nick, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 2, in->user, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 3, in->server, 64);
		pcre_copy_substring(msg, offsets, offsetcount, 4, in->channel, 32);
		// User that got kicked
		pcre_copy_substring(msg, offsets, offsetcount, 5, in->message, 32);
		if (strcmp(in->message, nick) == 0) {
			printf("Got kicked, rejoining\n");
			char rejoin[40];
			sprintf(rejoin, "JOIN #%s\n", channel);
			send_str(socket_fd, rejoin);
		}
		return;
	}

}

size_t http_write_callback(void *contents, size_t element_size, size_t num_elements, void *userpointer) {
    size_t size = element_size * num_elements;

    if (size + http_buffer_pos > HTTP_BUFFER) {
        size = HTTP_BUFFER - http_buffer_pos;
    }
    if (size < 0) {
        return 0;
    }

    memcpy(&http_buffer[http_buffer_pos], contents, size);
    http_buffer_pos += size;
//    return size;
    return element_size * num_elements;
}

void handle_input(struct recv_data *in, struct patterns *patterns) {
    const char *msg = in->message;
	int offsets[30];
	int offsetcount = 30;
	offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), 0, 0, offsets, offsetcount);
    if (offsetcount > 0) {
        for (int i=1; i<offsetcount; i++) {
            char url[BUFFER];
		    pcre_copy_substring(msg, offsets, offsetcount, i, url, BUFFER);
            printf("Got url: %s\n", url);

            http_buffer_pos = 0;
            CURL *curl_handle = curl_easy_init();
            curl_easy_setopt(curl_handle, CURLOPT_URL, url);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &http_write_callback);
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
            curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
            curl_easy_perform(curl_handle);
            curl_easy_cleanup(curl_handle);

            int titles[1];
	        int titlecount = pcre_exec(patterns->html_title, 0, http_buffer, HTTP_BUFFER, 0, 0, titles, 1);
            char title[BUFFER];
            printf("%s\n", http_buffer);
            printf("matches: %d\n", titlecount);
            if (titlecount > 0) {
		        pcre_copy_substring(http_buffer, titles, titlecount, 1, title, BUFFER);
                printf("%s\n", title);
                char *buf = malloc(strlen(title) + strlen(in->nick) + 10 + 4);
                sprintf(buf, "PRIVMSG %s :>> %s\n", in->nick, title);
                send_str(socket_fd, buf);
                free(buf);
            }
        }
    }
}

void send_str(int socket_fd, char *msg)
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

	pthread_mutex_unlock(send_mutex);
	pthread_mutex_unlock(send_sleep_mutex);

	// Print out what we have done
	printf("--> %s", msg);
}

void *send_loop(void *arg)
{
	while (send_thread_running) {
		pthread_mutex_lock(send_sleep_mutex);
		pthread_mutex_lock(send_mutex);
		while (send_buffer_used > 0) {
			int sent = send(socket_fd, send_buffer, send_buffer_used, 0);
			if (sent == -1) {
				die("Unable to send", strerror(errno));
			}
			send_buffer_used -= sent;
		}
		pthread_mutex_unlock(send_mutex);
	}
	return 0;
}

char *read_line(FILE *file)
{
	char line_buf[BUFFER];
	fgets(line_buf, BUFFER, file);

	int length = strlen(line_buf);
	// Remove trailing newline
	line_buf[length-1] = '\0';
	char *line = malloc(length);
	strncpy(line, line_buf, length);
	return line;
}

int load_config()
{
	FILE *config_file;

	config_file = fopen("cbot.conf", "r");
	if (config_file == 0) {
		printf("Unable to open config file: cbot.conf\n");
		exit(1);
	}

	nick = read_line(config_file); 
	user = read_line(config_file); 
	host = read_line(config_file); 
	port = read_line(config_file); 
	channel = read_line(config_file); 
	return 1;
}



int main(int argc, char **argv)
{
	if (argc == 1) {
		load_config();
	}
	else if (argc == 6) {
		nick    = argv[1];
		user    = argv[2];
		host    = argv[3];
		port    = argv[4];
		channel = argv[5];
	}
	else {
		printf("Usage: %s nick user host port channel\n", argv[0]);
		exit(0);
	}
	printf("nick: %s, user: %s, host: %s, port: %s, channel: %s\n",
		nick, user, host, port, channel);

    // Set up cURL
    curl_global_init(CURL_GLOBAL_ALL);

	int err, recv_size;
	char buffer[BUFFER];
	struct addrinfo hints;
	struct addrinfo *srv;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	// Connect
	if ((err = getaddrinfo(host, port, &hints, &srv)) != 0)
		die("getaddrinfo", gai_strerror(err));
	if ((socket_fd = socket(srv->ai_family, srv->ai_socktype, 0)) < 0)
		die("socket", gai_strerror(socket_fd));
	if ((err = connect(socket_fd, srv->ai_addr, srv->ai_addrlen)) != 0)
		die("connect", gai_strerror(err));

	// Allocate the send buffer
	send_buffer = malloc(BUFFER);
	send_buffer_size = BUFFER;

	// Create our mutexes
	send_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	send_sleep_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(send_mutex, 0);
	pthread_mutex_init(send_sleep_mutex, 0);

	// Create our sending thread
	send_thread = (pthread_t*)malloc(sizeof(pthread_t));
	send_thread_running = 1;
	pthread_create(send_thread, 0, &send_loop, 0);

	// Select param
	fd_set socket_set;
	FD_ZERO(&socket_set);
	FD_SET(STDIN_FILENO, &socket_set);
	FD_SET(socket_fd, &socket_set);

	// Join
	sprintf(buffer, "USER %s host realmname :%s\nNICK %s\nJOIN #%s\n", user, nick, nick, channel);
	send_str(socket_fd, buffer);

	struct recv_data *irc = malloc(sizeof(*irc));
	struct patterns *patterns = malloc(sizeof(*patterns));
	compile_patterns(patterns);

	while (select(socket_fd+1, &socket_set, 0, 0, 0) != -1) {
		if (FD_ISSET(STDIN_FILENO, &socket_set)) {
			char input[BUFFER];
			fgets(input, BUFFER, stdin);
			printf("you wrote: %s", input);
			FD_SET(socket_fd, &socket_set);
		}
		else {
			recv_size = recv(socket_fd, buffer, BUFFER-1, 0);
			// Add \0 to terminate string
			buffer[recv_size] = '\0';
			printf("%s", buffer);	
			parse_input(buffer, irc, patterns);
            handle_input(irc, patterns);
			FD_SET(STDIN_FILENO, &socket_set);
		}
	}

	close(socket_fd);
    curl_global_cleanup();
	free(irc);
	pcre_free(patterns->privmsg);
	pcre_free(patterns->kick);
	free(patterns);
	free(nick);
	free(user);
	free(host);
	free(port);
	free(channel);

	return 0;
}

// vim: set formatoptions+=ro cindent noexpandtab
