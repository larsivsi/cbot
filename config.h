#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>

#define CONFIG_FILE "cbot.conf"

struct config {
	char *nick;
	char *user;
	char *host;
	char *port;
	char *channel;
};
extern struct config *config;

char *read_line(FILE *file);
int load_config(void);

#endif//CONFIG_H
