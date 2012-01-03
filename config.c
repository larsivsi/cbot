#include "config.h"

#include "main.h"

#include <stdlib.h>
#include <string.h>

struct config *config;

char *read_line(FILE *file)
{
	char line_buf[BUFFER];
	fgets(line_buf, BUFFER, file);

	int length = strlen(line_buf);
	// Remove trailing newline
	line_buf[length-1] = '\0';
	char *line = malloc(length);
	strncpy(line, line_buf, length);
	return line;
}

int load_config(void)
{
	FILE *config_file;

	config_file = fopen(CONFIG_FILE, "r");
	if (config_file == 0) {
		printf("Unable to open config file: %s\n", CONFIG_FILE);
		exit(1);
	}

	config->nick = read_line(config_file); 
	config->user = read_line(config_file); 
	config->host = read_line(config_file); 
	config->port = read_line(config_file); 
	config->channel = read_line(config_file); 
	return 1;
}
