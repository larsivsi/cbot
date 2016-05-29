#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

#define MODULE_EIGHTBALL  1<<0
#define MODULE_URLS       1<<1
#define MODULE_TIMER      1<<2
#define MODULE_MARKOV     1<<3
#define MODULE_AUTOOP     1<<4
#define MODULE_LOG	  1<<5

struct config {
	char *nick;
	char *user;
	char *host;
	char *port;
	char **channels;
	char *db_connection_string;
	char **ops;
	char *markovcorpus;
	int enabled_modules;
};
extern struct config *config;

int load_config(const char *file);
void free_config();

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//CONFIG_H
