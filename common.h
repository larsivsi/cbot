#ifndef COMMON_H
#define COMMON_H

// generic buffer size
#define BUFFER_SIZE 4096

extern int socket_fd;

typedef struct real_pcre pcre;

struct patterns {
	pcre *privmsg;
	pcre *join;
	pcre *kick;
	pcre *url;
	pcre *html_title;

	pcre *command_eightball;
	pcre *command_uptime;
	pcre *command_say;
	pcre *command_kick;
	pcre *command_op;

	pcre *command_timer;
	pcre *time_offset;
	pcre *time_hourminute;
	pcre *time_timedate;
	pcre *time_daytime;
};
extern struct patterns *patterns;
void clean_spaces(char *str);
void strip_newlines(char *str);

// Prototypes
void die(const char *msg, const char *err);
long toc();

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//COMMON_H
