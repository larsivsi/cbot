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
	sprintf(buf, "PRIVMSG %s :%s: %s!\n", object->channel, object->nick, object->message);
	irc_send_str(buf);
	free(object->thread);
	free(object);

	return 0;
}

const char *days[] = {
	"sun",
	"mon",
	"tue",
	"wed",
	"thu",
	"fri",
	"sat"
};

void set_timer(const char *nick, const char *channel, const char *message, unsigned long seconds)
{
	// Tell the user how long he has to wait
	time_t t = time(NULL) + seconds;
	char timedatebuf[BUFFER_SIZE];
	strftime(timedatebuf, BUFFER_SIZE, "%a %d.%m.%Y %H:%M:%S", localtime(&t));
	char buf[BUFFER_SIZE];
	snprintf(buf, BUFFER_SIZE, "PRIVMSG %s :Notifying you in %lu seconds (%s) \n", channel, seconds, timedatebuf);
	irc_send_str(buf);

	// Create a thread that will sleep for the specified amount of time before suiciding
	pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
	timer_thread_t *object = (timer_thread_t*)(malloc(sizeof(timer_thread_t)));
	object->thread = thread;
	object->seconds = seconds;
	strncpy(object->nick, nick, 32);
	object->nick[31] = 0;
	strncpy(object->channel, channel, 32);
	object->channel[31] = 0;
	strncpy(object->message, message, BUFFER_SIZE);
	object->message[BUFFER_SIZE - 1] = 0;
	pthread_create(thread, 0, &timer_thread, object);
	pthread_detach(*thread);
}

void timer_parse(const char *nick, const char *channel, const char *time_str)
{
	printf("timestr: %s\n", time_str);
	int offsets[30];
	int offsetcount = pcre_exec(patterns->time_offset, 0, time_str, strlen(time_str), 0, 0, offsets, 30);

	time_t rawtime = time(NULL);
	struct tm *current_time = localtime(&rawtime);

	if (offsetcount == 4) {
		char count_str[5];
		pcre_copy_substring(time_str, offsets, offsetcount, 1, count_str, 5);
		unsigned long count = atoi(count_str);
		char type[2];
		pcre_copy_substring(time_str, offsets, offsetcount, 2, type, 2);
		char message[64];
		pcre_copy_substring(time_str, offsets, offsetcount, 3, message, 64);
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
	offsetcount = pcre_exec(patterns->time_hourminute, 0, time_str, strlen(time_str), 0, 0, offsets, 30);
	if (offsetcount == 4) {
		char hour_str[3];
		char minute_str[3];
		char message[64];
		pcre_copy_substring(time_str, offsets, offsetcount, 1, hour_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 2, minute_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 3, message, 64);
		int hour = atoi(hour_str);
		if (hour > 23) {
			printf("invalid hour: %s\n" , hour_str);
			return;
		}
		int minute = atoi(minute_str);
		if (minute > 59) {
			printf("invalid minute: %s\n", minute_str);
			return;
		}

		int hours = hour - current_time->tm_hour;
		int minutes = minute - current_time->tm_min;
		if (hours < 0) {
			hours += 24;
		}
		if (minutes < 0) {
			minutes += 60;
		}
		printf("hours: %d minutes: %d\n", hours, minutes);
		int seconds = hours * 3600 + minutes * 60;
		set_timer(nick, channel, message, seconds);
		return;
	}
	offsetcount = pcre_exec(patterns->time_timedate, 0, time_str, strlen(time_str), 0, 0, offsets, 30);
	if (offsetcount == 6) {
		char day_str[3];
		char month_str[3];
		char hour_str[3];
		char minute_str[3];
		char message[64];
		pcre_copy_substring(time_str, offsets, offsetcount, 1, day_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 2, month_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 3, hour_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 4, minute_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 5, message, 64);
		int day = atoi(day_str);
		if (day > 31) {
			printf("invalid day: %s\n", day_str);
			return;
		}
		int month = atoi(month_str);
		if (month < 1 || month > 12) {
			printf("invalid month: %s\n", month_str);
			return;
		}
		month--; // 0-indexed...
		int hour = atoi(hour_str);
		if (hour > 23) {
			printf("invalid hour: %s\n" , hour_str);
			return;
		}
		int minute = atoi(minute_str);
		if (minute > 59) {
			printf("invalid minute: %s\n", minute_str);
			return;
		}
		struct tm target_time;
		target_time.tm_sec = current_time->tm_sec;
		target_time.tm_min = minute;
		target_time.tm_hour = hour;
		target_time.tm_mday = day;
		target_time.tm_mon = month;
		target_time.tm_year = current_time->tm_year;
		target_time.tm_isdst = -1; // mktime should figure it out for itself
		if (mktime(&target_time) - mktime(current_time) <= 0) {
			target_time.tm_year++;
		}
		int seconds = difftime(mktime(&target_time), mktime(current_time));
		set_timer(nick, channel, message, seconds);
		return;
	}
	offsetcount = pcre_exec(patterns->time_daytime, 0, time_str, strlen(time_str), 0, 0, offsets, 30);
	if (offsetcount == 5) {
		char day_str[16];
		char hour_str[3];
		char minute_str[3];
		char message[64];
		pcre_copy_substring(time_str, offsets, offsetcount, 1, day_str, 16);
		pcre_copy_substring(time_str, offsets, offsetcount, 2, hour_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 3, minute_str, 3);
		pcre_copy_substring(time_str, offsets, offsetcount, 4, message, 64);
		int day;
		for (day = 0; day < 7; day++) {
			if (strncmp(day_str, days[day], 3) == 0) {
				break;
			}
		}
		int days = day - current_time->tm_wday;
		if (days <= 0) {
			days += 7;
		}
		int hour = atoi(hour_str);
		if (hour > 23) {
			printf("invalid hour: %s\n" , hour_str);
			return;
		}
		int minute = atoi(minute_str);
		if (minute > 59) {
			printf("invalid minute: %s\n", minute_str);
			return;
		}
		int hours = hour - current_time->tm_hour;
		int minutes = minute - current_time->tm_min;
		if (hours < 0) {
			hours += 24;
		}
		if (minutes < 0) {
			minutes += 60;
		}
		printf("days: %d, hour: %d, min: %d\n", days, hours, minutes);
		int seconds = days * 86400 + hours * 3600 + minutes * 60;
		set_timer(nick, channel, message, seconds);
		return;
	}
	char buf[strlen(channel) + 37/*constant text*/ + 11 /*maximum length of the unsigned int*/];
	sprintf(buf, "PRIVMSG %s :timer example: \"!timer 17m pizza!\", time syntax examples: 1d, 10m, 13:37, 24/12-13:37, monday-13:37\n", channel);
	irc_send_str(buf);
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
