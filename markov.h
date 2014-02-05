#ifndef MARKOV_H
#define MARKOV_H

#define HASHTABLE_SIZE 18220

struct recv_data; // defined in main.h

void markov_init(const char *corpus_file);
void markov_parse(struct recv_data *in);
void markov_terminate();

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//MARKOV_H
