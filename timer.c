#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <pcre.h>

#include "timer.h"
#include "common.h"
#include "irc.h"

typedef struct
{
	pthread_t *thread;
	char nick[32];
	char channel[32];
	char message[BUFFER_SIZE];
	unsigned int seconds;
} timer_thread_t;

static void *timer_thread(void *argument)
{
	timer_thread_t *object = (timer_thread_t*)argument;
	sleep(object->seconds);
	char buf[strlen(object->channel) + strlen(object->nick) + 18];
	sprintf(buf, "PRIVMSG %s :%s: DING!\n", object->channel, object->nick);
	irc_send_str(buf);
	free(object->thread);
	free(object);

	return 0;
}

void set_timer(const char *nick, const char *channel, const char *message, unsigned long seconds)
{
	// Tell the user how long he has to wait
	char buf[strlen(channel) + 37/*constant text*/ + 11 /*maximum length of the unsigned int*/];
	sprintf(buf, "PRIVMSG %s :Notifying you in %lu seconds\n", channel, seconds);
	irc_send_str(buf);

	// Create a thread that will sleep for the specified amount of time before suiciding
	pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
	timer_thread_t *object = (timer_thread_t*)(malloc(sizeof(timer_thread_t)));
	object->thread = thread;
	object->seconds = seconds;
	strncpy(object->nick, nick, 32);
	strncpy(object->channel, channel, 32);
	strncpy(object->message, message, BUFFER_SIZE);
	pthread_create(thread, 0, &timer_thread, object);
	pthread_detach(*thread);
}

void timer_parse(const char *nick, const char *channel, const char *time)
{
	int offsets[30];
	int offsetcount = pcre_exec(patterns->time_offset, 0, time, strlen(time), 0, 0, offsets, 30);
	if (offsetcount == 4) {
		char count_str[5];
		pcre_copy_substring(time, offsets, offsetcount, 1, count_str, 5);
		unsigned long count = atoi(count_str);
		char type[2];
		pcre_copy_substring(time, offsets, offsetcount, 2, type, 2);
		char message[BUFFER_SIZE];
		pcre_copy_substring(time, offsets, offsetcount, 3, message, BUFFER_SIZE);
		unsigned long seconds;
		switch(type[0]) {
		case 'w':
			seconds = count * 604800;
			break;
		case 'd':
			seconds = count * 86400;
			break;
		case 'h':
			seconds = count * 3600;
			break;
		case 'm':
			seconds = count * 60;
			break;
		case 's':
			seconds = count;
			break;
		default:
			printf("invalid time type: %s\n", type);
			return;
		}
		set_timer(nick, channel, message, seconds);
		return;
	}

}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
