#ifndef TIMER_H
#define TIMER_H

#include <time.h>

#define TIMER_MESSAGE_SIZE 64

void timer_parse(const char *nick, const char *channel, const char *time);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//TIMER_H
