#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "timer.h"
#include "main.h"

typedef struct
{
    pthread_t *thread;
    const char *nick;
    const char *channel;
    unsigned int seconds;
} timer_thread_t;

static void *timer_thread(void *argument)
{
    timer_thread_t *object = (timer_thread_t*)argument;
    printf("sleeping for %d seconds\n", object->seconds);
    sleep(object->seconds);
    char buf[strlen(object->channel) + strlen(object->nick) + 15];
    sprintf(buf, "PRIVMSG %s :%s DING!\n", object->channel, object->nick);
    send_str(buf);
    free(object->thread);
    free(object);
    return 0;
}

void set_timer(const char *nick, const char *channel, unsigned int seconds)
{
    printf("setting timer\n");
    pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
    timer_thread_t *object = (timer_thread_t*)(malloc(sizeof(timer_thread_t)));
    object->thread = thread;
    object->nick = nick;
    object->seconds = seconds;
    object->channel = channel;
    pthread_create(thread, 0, &timer_thread, object);
}


