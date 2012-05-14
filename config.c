#include "config.h"

#include "main.h"

#include <pcre.h>
#include <stdlib.h>
#include <string.h>

struct config *config;

char *read_line(FILE *file)
{
	char line_buf[BUFFER];
	if (fgets(line_buf, BUFFER, file) == 0)
		return 0;
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

	const char *pcre_err;
	int pcre_err_off;
	pcre *pattern;
	if ((pattern = pcre_compile("(\\w+)\\s*=\\s*(.+)", PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL) {
		printf("Could not compile config pattern\n");
		exit(1);
	}

	char *line;
	char *parameter = malloc(BUFFER);
	char *value = malloc(BUFFER);
	while((line = read_line(config_file)) != 0) {
		// TODO: check 30
		int offsets[30];
		int offsetcount = pcre_exec(pattern, 0, line, strlen(line), 0, 0, offsets, 30);
		if (offsetcount == 3) {
			pcre_copy_substring(line, offsets, offsetcount, 1, parameter, BUFFER);
			pcre_copy_substring(line, offsets, offsetcount, 2, value, BUFFER);
			set_config_param(parameter, value);
		}
		else {
			printf("Illegal line in config file: \'%s\'!\n", line);
			exit(1);
		}
		free(line);
	}

	pcre_free(pattern);
	free(parameter);
 	free(value);

	fclose(config_file);
	return 1;
}

void set_config_param(char *parameter, char *value) {
	if (!strcmp(parameter, "nick")) {
		config->nick = malloc(strlen(value) + 1);
		strcpy(config->nick, value);
	}
	else if (!strcmp(parameter, "user")) {
		config->user = malloc(strlen(value) + 1);
		strcpy(config->user, value);
	}
	else if (!strcmp(parameter, "host")) {
		config->host = malloc(strlen(value) + 1);
		strcpy(config->host, value);
	}
	else if (!strcmp(parameter, "port")) {
		config->port = malloc(strlen(value) + 1);
		strcpy(config->port, value);
	}
	else if (!strcmp(parameter, "channel")) {
		config->channel = malloc(strlen(value) + 1);
		strcpy(config->channel, value);
	}
	else if (!strcmp(parameter, "dbconnect")) {
		config->db_connection_string = malloc(strlen(value) + 1);
		strcpy(config->db_connection_string, value);
	}
	else {
		printf("WARNING: Unknown config parameter \'%s\' with value \'%s\'!\n", parameter, value);
	}
}
/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
