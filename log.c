#include "main.h"
#include "config.h"

#include <string.h>
#include <libpq-fe.h>

PGconn *connection = 0;

void log_abort()
{
	PQfinish(connection);
	exit(1);
}

void log_init()
{
	connection = PQconnectdb(config->db_connection_string);
	if (PQstatus(connection) != CONNECTION_OK) {
		printf("Unable to connect to database!");
		log_abort();
	}

	PGresult *result;

	result = PQprepare(connection, "create_nick", "INSERT INTO nicks(nick, name, created_at, updated_at, words) VALUES ($1, '', NOW(), NOW(), 0) RETURNING id", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf("Unable to create prepared function (create_nick)!");
		log_abort();
	}

	result = PQprepare(connection, "log_message", "INSERT INTO logs(nick_id, text, created_at) VALUES ($1, $2, NOW())", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf("Unable to create prepared function (log_message)!");
		log_abort();
	}

	result = PQprepare(connection, "update_nick", "UPDATE nicks SET updated_at=NOW(), words=words+$1 WHERE nick=$2", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf("Unable to create prepared function (update_nick)!");
		log_abort();
	}

	result = PQprepare(connection, "get_nick_id", "SELECT id FROM nicks WHERE nick=$1", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf("Unable to create prepared function (get_nick_id)!");
		log_abort();
	}

	printf(":: Database connected, logging enabled!\n");
}

void log_terminate()
{
	PQfinish(connection);
}

int count_words(const char *string)
{
	int words = 1;
	for (int i=0; i<strlen(string); i++) {
		if (string[i] == ' ') {
			words++;
			while (string[i] == ' ') {
				i++;
				if (string[i] == '\0') return words;
			}
		}
	}
	return words;
}

void log_message(struct recv_data *in)
{
	if (strcmp(in->nick, "") == 0) return;

	const char *parameters[2];
	PGresult *result;

	parameters[0] = in->nick;
	result = PQexecPrepared(connection, "get_nick_id", 1, parameters, 0, 0, 0);
	if (PQresultStatus(result) != PGRES_TUPLES_OK) {
		printf("Unable to get nick id from database!");
		return;
	}
	if (PQntuples(result) == 0) { //nick doesn't exist, create it
		result = PQexecPrepared(connection, "create_nick", 1, parameters, 0, 0, 0);
		if (PQresultStatus(result) != PGRES_TUPLES_OK) {
			printf("Unable to create nick in database! (%s)\n",  PQerrorMessage(connection));
			return;
		}
	}

	parameters[0] = PQgetvalue(result, 0, 0);//nick_id
	parameters[1] = in->message;
	result = PQexecPrepared(connection, "log_message", 2, parameters, 0, 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf("Unable to log message in database (%s)!\n", PQerrorMessage(connection));
		return;
	}

	char *numwords_buf = malloc(sizeof(int)*8+1);
	sprintf(numwords_buf, "%d", count_words(in->message));
	parameters[0] = numwords_buf;
	parameters[1] = in->nick;
	result = PQexecPrepared(connection, "update_nick", 2, parameters, 0, 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf("Unable to log message in database (%s)!\n", PQerrorMessage(connection));
		return;
	}


	free(numwords_buf);

}
/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
