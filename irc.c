#include "irc.h"

#include "config.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <pcre.h>

char *send_buffer = 0;
size_t send_buffer_size = 0;
size_t send_buffer_used = 0;
pthread_mutex_t *send_mutex = 0;
pthread_cond_t *send_data_ready = 0;
pthread_barrier_t *startup_barr = 0;
pthread_t *send_thread = 0;
int send_thread_running = 0;

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
		printf("lol: %s\n", msg);
		pcre_copy_substring(msg, offsets, offsetcount, 1, in->nick, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 2, in->user, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 3, in->server, 64);
		pcre_copy_substring(msg, offsets, offsetcount, 4, in->channel, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 5, in->message, BUFFER_SIZE);
		// In case of privmsgs
		if (strcmp(in->channel, config->nick) == 0) {
			printf("%s : %s \n", in->channel, in->nick);
			strcpy(in->channel, in->nick);
			printf("%s : %s \n", in->channel, in->nick);
		}
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

void send_str(char *msg)
{
	pthread_mutex_lock(send_mutex);

	int length = strlen(msg);
	// Check if we have enough space
	if (length > send_buffer_size - send_buffer_used) {
		int new_buffer_size = send_buffer_size + BUFFER_SIZE;
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
	printf("%s", msg);
}

void *send_loop(void *arg)
{
	pthread_mutex_lock(send_mutex);
	// Tell main thread that mutex is locked
	pthread_barrier_wait(startup_barr);
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

void irc_init()
{
	// Allocate the send buffer
	send_buffer = malloc(BUFFER_SIZE);
	send_buffer_size = BUFFER_SIZE;

	// Create our mutexes
	send_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	send_data_ready = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	startup_barr = (pthread_barrier_t*)malloc(sizeof(pthread_barrier_t));
	pthread_mutex_init(send_mutex, 0);
	pthread_cond_init(send_data_ready, 0);
	// Create a barrier for main and sending threads
	pthread_barrier_init(startup_barr, 0, 2);

	// Create our sending thread
	send_thread = (pthread_t*)malloc(sizeof(pthread_t));
	send_thread_running = 1;
	pthread_create(send_thread, 0, &send_loop, 0);
	// Wait for sending thread to get mutex
	pthread_barrier_wait(startup_barr);

}

void irc_terminate()
{
	send_thread_running = 0;
	pthread_cond_signal(send_data_ready);
	pthread_mutex_unlock(send_mutex);
	pthread_join(*send_thread, 0);
	free(send_thread);

	pthread_mutex_destroy(send_mutex);
	free(send_mutex);
	pthread_cond_destroy(send_data_ready);
	free(send_data_ready);
	pthread_barrier_destroy(startup_barr);
	free(startup_barr);
	free(send_buffer);
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
