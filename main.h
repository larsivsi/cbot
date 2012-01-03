#ifndef MAIN_H
#define MAIN_H

// For getaddrinfo and the likes
#define _POSIX_C_SOURCE 200112L

#include <pthread.h>
#include <pcre.h>

// Buffers
#define BUFFER 512

// Structs
struct recv_data {
	char nick[32];
	char user[32];
	char server[64];
	char channel[32];
	char message[BUFFER];
};


struct patterns {
	pcre *privmsg;
	pcre *kick;
	pcre *url;
	pcre *html_title;
};
extern struct patterns *patterns;

// Prototypes
void compile_patterns(struct patterns *patterns);
void die(const char *msg, const char *err);
int parse_input(char *msg, struct recv_data *in, struct patterns *patterns);
void send_str(char *msg);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//MAIN_H
