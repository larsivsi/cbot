#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

struct config {
	char *nick;
	char *user;
	char *host;
	char *port;
	char *channels;
	char *db_connection_string;
	char **ops;
};
extern struct config *config;

int load_config(const char *file);
void free_config();

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//CONFIG_H
