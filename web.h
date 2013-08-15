#ifndef TITLE_H
#define TITLE_H

struct recv_data; // defined in main.h

void get_title_from_url(struct recv_data *in, const char *url);
void get_last_tweet_from_user(struct recv_data *in, const char *user);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//TITLE_H
