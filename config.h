#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

struct config {
	char *nick;
	char *user;
	char *host;
	char *port;
	char *channel;
	char *db_connection_string;
};
extern struct config *config;

int read_line(FILE *file, char *line);
int load_config(const char *file);
void set_config_param(char *parameter, char *value);

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
#endif//CONFIG_H
