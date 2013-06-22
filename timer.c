#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "timer.h"
#include "common.h"
#include "irc.h"

typedef struct
{
	pthread_t *thread;
	char nick[32];
	char channel[32];
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

void set_timer(const char *nick, const char *channel, unsigned int seconds)
{
	// Tell the user how long he has to wait
	char buf[strlen(channel) + 37/*constant text*/ + 11 /*maximum length of the unsigned int*/];
	sprintf(buf, "PRIVMSG %s :Notifying you in %d seconds\n", channel, seconds);
	irc_send_str(buf);

	// Create a thread that will sleep for the specified amount of time before suiciding
	pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
	timer_thread_t *object = (timer_thread_t*)(malloc(sizeof(timer_thread_t)));
	object->thread = thread;
	object->seconds = seconds;
	strncpy(object->nick, nick, 32);
	strncpy(object->channel, channel, 32);
	pthread_create(thread, 0, &timer_thread, object);
	pthread_detach(*thread);
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
