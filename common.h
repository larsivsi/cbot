#ifndef COMMON_H
#define COMMON_H

#include <pcre.h>

// Buffers
#define BUFFER 512

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
void compile_patterns(struct patterns *patterns);
void die(const char *msg, const char *err);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//COMMON_H
