#ifndef COMMON_H
#define COMMON_H

// generic buffer size
#define BUFFER_SIZE 4096

extern int socket_fd;

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

struct patterns {
	pcre2_code *privmsg;
	pcre2_code *join;
	pcre2_code *kick;
	pcre2_code *url;
	pcre2_code *html_title;

	pcre2_code *command_eightball;
	pcre2_code *command_uptime;
	pcre2_code *command_say;
	pcre2_code *command_kick;
	pcre2_code *command_op;

	pcre2_code *command_timer;
	pcre2_code *time_offset;
	pcre2_code *time_hourminute;
	pcre2_code *time_timedate;
	pcre2_code *time_daytime;
};
extern struct patterns *patterns;
void clean_spaces(char *str);
void strip_newlines(char *str);

// Prototypes
void die(const char *msg, const char *err);
long toc();

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//COMMON_H
