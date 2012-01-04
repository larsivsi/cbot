#ifndef TIMER_H
#define TIMER_H

#include <time.h>

struct recv_data;

void set_timer(const char *nick, const char *channel, int seconds);

#endif//TIMER_H
