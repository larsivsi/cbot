#include "irc.h"

#include "config.h"
#include "eightball.h"
#include "log.h"
#include "title.h"
#include "timer.h"

#include <errno.h>
#include <pcre.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

char *send_buffer = 0;
size_t send_buffer_size = 0;
size_t send_buffer_used = 0;
pthread_mutex_t *send_mutex = 0;
pthread_cond_t *send_data_ready = 0;
pthread_barrier_t *startup_barr = 0;
pthread_t *send_thread = 0;
int send_thread_running = 0;

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

// Returns 1 on privmsg, 0 otherwise
int irc_parse_input(char *msg, struct recv_data *in, struct patterns *patterns)
{
	// Ping
	if (strncmp(msg, "PING :", 6) == 0) {
		// Turn the ping into a pong :D
		msg[1] = 'O';
		irc_send_str(msg);
		return 0;
	}
	// Normal message
	//TODO: check 30
	int offsets[30];
	int offsetcount = pcre_exec(patterns->privmsg, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount == 6) {
		pcre_copy_substring(msg, offsets, offsetcount, 1, in->nick, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 2, in->user, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 3, in->server, 64);
		pcre_copy_substring(msg, offsets, offsetcount, 4, in->channel, 32);
		pcre_copy_substring(msg, offsets, offsetcount, 5, in->message, BUFFER_SIZE);
		// In case of privmsgs ### fuck privmsgs for now -- sandsmark
/*		if (strcmp(in->channel, config->nick) == 0) {
			printf("%s : %s \n", in->channel, in->nick);
			strcpy(in->channel, in->nick);
			printf("%s : %s \n", in->channel, in->nick);
		}*/
		return 1;
	}
	// Kick
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
			sprintf(rejoin, "JOIN %s\n", in->channel);
			irc_send_str(rejoin);
		}
		return 0;
	}

	// Join
	offsetcount = pcre_exec(patterns->join, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		char nick[BUFFER_SIZE];
		char identity[BUFFER_SIZE];
		char channel[BUFFER_SIZE];
		pcre_copy_substring(msg, offsets, offsetcount, 1, nick, BUFFER_SIZE);
		pcre_copy_substring(msg, offsets, offsetcount, 2, identity, BUFFER_SIZE);
		pcre_copy_substring(msg, offsets, offsetcount, 3, channel, BUFFER_SIZE);

		int valid = 0;
		int i=0;

		while (config->ops[i]) {
			if (!strcmp(config->ops[i++], identity)) {
				valid = 1;
				break;
			}
		}

		if (valid) {
			char buf[strlen("MODE  +o \n") + strlen(channel) + strlen(nick)];
			sprintf(buf, "MODE %s +o %s\n", channel, nick); 
			irc_send_str(buf);
		}
	}
	return 0;
}

void irc_send_str(char *msg)
{
	pthread_mutex_lock(send_mutex);

	unsigned int length = strlen(msg);
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
	(void)arg; // We don't need this from pthread

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

void irc_handle_input(struct recv_data *in, struct patterns *patterns)
{
	log_message(in);

	const char *msg = in->message;
	int offsets[30];
	// Check URLs
	int offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), 0, 0, offsets, 30);
	while (offsetcount > 0) {
		char url[BUFFER_SIZE];
		pcre_copy_substring(msg, offsets, offsetcount, 1, url, BUFFER_SIZE);
		get_title_from_url(in, url);
		log_url(in, url);
		offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), offsets[1], 0, offsets, 30);
	}

	// 8ball
	offsetcount = pcre_exec(patterns->command_eightball, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		char arguments[BUFFER_SIZE];
		pcre_copy_substring(msg, offsets, offsetcount, 1, arguments, BUFFER_SIZE);
		eightball(in, arguments);
	}

	// Timer
	offsetcount = pcre_exec(patterns->command_timer, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		// We limit at 4 digits
		char time[4];
		pcre_copy_substring(msg, offsets, offsetcount, 1, time, 4);
		int seconds = atoi(time)*60;
		set_timer(in->nick, in->channel, seconds);
	}

	// Uptime
	offsetcount = pcre_exec(patterns->command_uptime, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		long elapsed_secs = toc() / 1000000;
		char *buf = (char*)malloc(BUFFER_SIZE);
		char *stf = (char*)malloc(BUFFER_SIZE);
		uptime_in_stf(elapsed_secs, stf);
		sprintf(buf, "PRIVMSG %s :I have been operational for %ld seconds, aka. %s\n",
			in->channel, elapsed_secs, stf);
		irc_send_str(buf);
		free(buf);
		free(stf);
	}

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
