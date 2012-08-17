#ifndef IRC_H
#define IRC_H

#include "common.h"

struct recv_data {
	char nick[32];
	char user[32];
	char server[64];
	char channel[32];
	char message[BUFFER_SIZE];
};

void irc_init();
void irc_terminate();
int parse_input(char *msg, struct recv_data *in, struct patterns *patterns);
void send_str(char *msg);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//IRC_H
