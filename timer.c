#include "timer.h"

#include "main.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>


typedef struct {
    struct timer *next;
    struct timer *previous;
    int id;
    char *nick;
    char *channel;
} timer;

timer *head_timer;

void timer_init()
{
}

/*void timer_handler(timer_t timer, int id)
{
    for (timer *t = head_timer; t != 0; t = t->next) {
        if (t->id == id) {
            // We aren't going to be used again
	    if (t->previous != 0)
	            t->previous->next = t->next;
	    if (t->next != 0)
	            t->next->previous = t->previous;


            char *buf = malloc(strlen(t->channel) + strlen(t->nick) + 26);
            sprintf(buf, "PRIVMSG %s :>> %s: Time's up!\n", t->channel, t->nick);
            send_str(buf);
            free(buf);

            // Remove ourselves
            free(t->nick);
            free(t->channel);
            free(t);
        }
    }
}*/

void set_timer(const char *nick, const char *channel, int seconds)
{
	/*timer_t timerid;
	struct itimerspec value;

	int id = rand();

	timer *new_timer = malloc(sizeof(new_timer));
	new_timer->id = id;
	new_timer->previous = 0;
	new_timer->next = head_timer;
	if (head_timer != 0) head_timer->previous = new_timer;
	head_timer = new_timer;

	value.it_value.tv_sec = seconds;
	value.it_value.tv_nsec = 0;

	value.it_interval.tv_sec = 0;
	value.it_interval.tv_nsec = 0;

	timer_create(CLOCK_REALTIME, NULL, &timerid);
	timer_connect(timerid, timer_handler, id);
	timer_settime(timerid, 0, &value, NULL);*/
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
