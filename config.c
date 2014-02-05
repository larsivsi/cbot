#include "config.h"

#include "common.h"

#include <pcre.h>
#include <stdlib.h>
#include <string.h>

struct config *config;

void free_config()
{
	int i=0;
	while (config->ops[i]) free(config->ops[i++]);
	i=0;
	while (config->channels[i]) free(config->channels[i++]);
	free(config->nick);
	free(config->user);
	free(config->host);
	free(config->port);
	free(config->channels);
	free(config->db_connection_string);

	free(config->ops);
	free(config->markovcorpus);
	free(config);
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
	else if (!strcmp(parameter, "channels")) {
		int count = 1;
		for (int i = 0; value[i] != '\0'; i++) {
			if (value[i] == ',')
				count++;
		}
		config->channels = (char**)malloc(sizeof(char*) * (count+1));
		config->channels[count] = NULL;
		int i = 0;
		char *channel = strtok(value, ",");
		while (channel != NULL) {
			config->channels[i] = (char*)malloc(strlen(channel) + 1);
			strcpy(config->channels[i], channel);
			i++;
			channel = strtok(NULL, ",");
		}
	}
	else if (!strcmp(parameter, "dbconnect")) {
		config->db_connection_string = malloc(strlen(value) + 1);
		strcpy(config->db_connection_string, value);
	}
	else if (!strcmp(parameter, "markovcorpus")) {
		config->markovcorpus = malloc(strlen(value) + 1);
		strcpy(config->markovcorpus, value);
	}
	else if (!strcmp(parameter, "ops")) {
		int op_count = 1;
		for (int i=0; value[i] != '\0'; i++)
			if (value[i] == ',') op_count++;

		config->ops = malloc(sizeof(char*) * (op_count+1));
		config->ops[op_count] = NULL;
		char *op = strtok(value, ",");
		int i=0;
		while (op != NULL) {
			config->ops[i] = malloc(strlen(op) + 1);
			strcpy(config->ops[i], op);
			i++;
			op = strtok(NULL, ",");
		}
	}
	else {
		printf("WARNING: Unknown config parameter \'%s\' with value \'%s\'!\n", parameter, value);
	}
}

int read_line(FILE *file, char *line)
{
	char line_buf[BUFFER_SIZE];
	if (fgets(line_buf, BUFFER_SIZE, file) == 0)
		return 1;
	int length = strlen(line_buf);
	// Remove trailing newline
	line_buf[length-1] = '\0';
	strncpy(line, line_buf, length);
	return 0;
}

int load_config(const char *filename)
{
	FILE *config_file;
	config = malloc(sizeof(struct config));

	config_file = fopen(filename, "r");
	if (config_file == 0) {
		printf("Unable to open config file: %s\n", filename);
		exit(1);
	}

	const char *pcre_err;
	int pcre_err_off;
	pcre *pattern;
	if ((pattern = pcre_compile("(\\w+)\\s*=\\s*(.+)", PCRE_CASELESS | PCRE_UTF8, &pcre_err, &pcre_err_off, 0)) == NULL) {
		printf("Could not compile config pattern\n");
		exit(1);
	}

	char *line = malloc(BUFFER_SIZE);
	char *parameter = malloc(BUFFER_SIZE);
	char *value = malloc(BUFFER_SIZE);
	while(read_line(config_file, line) == 0) {
		// TODO: check 30
		int offsets[30];
		int offsetcount = pcre_exec(pattern, 0, line, strlen(line), 0, 0, offsets, 30);
		if (offsetcount == 3) {
			pcre_copy_substring(line, offsets, offsetcount, 1, parameter, BUFFER_SIZE);
			pcre_copy_substring(line, offsets, offsetcount, 2, value, BUFFER_SIZE);
			set_config_param(parameter, value);
		}
		else {
			printf("Illegal line in config file: \'%s\'!\n", line);
			exit(1);
		}
	}

	pcre_free(pattern);
	free(line);
	free(parameter);
 	free(value);

	fclose(config_file);
	return 1;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
