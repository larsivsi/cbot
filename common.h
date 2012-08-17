#ifndef COMMON_H
#define COMMON_H

// generic buffer size
#define BUFFER_SIZE 512

extern int socket_fd;

typedef struct real_pcre pcre;

struct patterns {
	pcre *privmsg;
	pcre *kick;
	pcre *url;
	pcre *html_title;
	pcre *eightball;
	pcre *timer;
};
extern struct patterns *patterns;

// Prototypes
void die(const char *msg, const char *err);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//COMMON_H
