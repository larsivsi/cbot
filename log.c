#include "log.h"

#include "config.h"
#include "common.h"
#include "irc.h"

#include <libpq-fe.h>
#include <string.h>
#include <stdlib.h>

PGconn *connection = 0;

void log_abort()
{
	PQfinish(connection);
	connection = 0;
}

void log_init()
{
	connection = PQconnectdb(config->db_connection_string);
	if (PQstatus(connection) != CONNECTION_OK) {
		printf(" ! Unable to connect to database!\n");
		log_abort();
		return;
	}

	PGresult *result;

	// User queries
	result = PQprepare(connection, "create_nick", "INSERT INTO nicks(nick, name, created_at, updated_at, words) VALUES ($1, '', NOW(), NOW(), 0) RETURNING id", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (create_nick)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	result = PQprepare(connection, "update_nick", "UPDATE nicks SET updated_at=NOW(), words=words+$1 WHERE nick=$2", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (update_nick)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	result = PQprepare(connection, "get_nick_id", "SELECT id FROM nicks WHERE nick=$1", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (get_nick_id)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	// URL queries
	result = PQprepare(connection, "create_url", "INSERT INTO links(nick_id, link, created_at, times_posted) VALUES ((SELECT id FROM nicks WHERE nick = $1), $2, NOW(), 1)", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf(PQresultErrorMessage(result));
		PQclear(result);
		printf(" ! Unable to create prepared function (create_url)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	result = PQprepare(connection, "update_url", "UPDATE links SET times_posted = times_posted+1 WHERE link = $1", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (update_url)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	result = PQprepare(connection, "get_old_url", "SELECT nick, link, links.created_at, times_posted FROM links, nicks WHERE nick_id = nicks.id AND link=$1", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (get_old_url)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	// Other queries
	result = PQprepare(connection, "log_message", "INSERT INTO logs(nick_id, text, created_at) VALUES ($1, $2, NOW())", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		printf(PQresultErrorMessage(result));
		PQclear(result);
		printf(" ! Unable to create prepared function (log_message)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	result = PQprepare(connection, "log_eightball", "INSERT INTO eightball_logs(nick_id,query,answer,created_at) VALUES ($1, $2, $3, NOW())", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (log_eightball)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	printf(":: Database connected, logging enabled!\n");
}

void log_terminate()
{
	if (connection == 0) return;
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
	if (connection == 0) return;
	if (strcmp(in->nick, "") == 0) return;

	const char *parameters[2];
	PGresult *nickResult, *logResult, *wordResult;

	parameters[0] = in->nick;
	nickResult = PQexecPrepared(connection, "get_nick_id", 1, parameters, 0, 0, 0);
	if (PQresultStatus(nickResult) != PGRES_TUPLES_OK) {
		printf(" ! Unable to get nick id from database!\n");
		return;
	}
	if (PQntuples(nickResult) == 0) { //nick doesn't exist, create it
		PQclear(nickResult);
		nickResult = PQexecPrepared(connection, "create_nick", 1, parameters, 0, 0, 0);
		if (PQresultStatus(nickResult) != PGRES_TUPLES_OK) {
			printf(" ! Unable to create nick in database! (%s)\n",  PQerrorMessage(connection));
			return;
		}
	}

	parameters[0] = PQgetvalue(nickResult, 0, 0);//nick_id
	parameters[1] = in->message;
	logResult = PQexecPrepared(connection, "log_message", 2, parameters, 0, 0, 0);
	if (PQresultStatus(logResult) != PGRES_COMMAND_OK) {
		printf(" ! Unable to log message in database (%s)!\n", PQerrorMessage(connection));
		return;
	}

	char *numwords_buf = malloc(sizeof(int)*8+1);
	sprintf(numwords_buf, "%d", count_words(in->message));
	parameters[0] = numwords_buf;
	parameters[1] = in->nick;
	wordResult = PQexecPrepared(connection, "update_nick", 2, parameters, 0, 0, 0);
	if (PQresultStatus(wordResult) != PGRES_COMMAND_OK) {
		printf(" ! Unable to log message in database (%s)!\n", PQerrorMessage(connection));
		return;
	}


	PQclear(nickResult);
	PQclear(logResult);
	PQclear(wordResult);

	free(numwords_buf);

}

void log_url(struct recv_data *in, const char *url)
{
	const char *parameters[2];
	PGresult *link_result;

	parameters[0] = url;
	// SELECT nick, link, links.created_at, times_posted FROM links, nicks WHERE nick_id = nicks.id AND link=$1
	link_result = PQexecPrepared(connection, "get_old_url", 1, parameters, 0, 0, 0);
	if (PQresultStatus(link_result) != PGRES_TUPLES_OK) {
		PQclear(link_result);
		printf(" ! Unable to get old URLs from database!\n");
		return;
	}

	if (PQntuples(link_result) > 0) {
		char *times_posted = PQgetvalue(link_result, 0, 3);
		char *nick = PQgetvalue(link_result, 0, 0);
		char *date = PQgetvalue(link_result, 0, 2);
		char buf[90 + strlen(in->channel) + strlen(in->nick) + strlen(times_posted) + strlen(nick) + strlen(date)];
		sprintf(buf, "PRIVMSG %s :%s: OLD! Your link has been posted %s times and was first posted by %s at %s\n",
				in->channel, 
				in->nick, 
				times_posted, 
				nick, 
				date);
		send_str(buf);
		PQclear(link_result);

		//UPDATE links SET times_posted = times_posted+1 WHERE link = $1
		link_result = PQexecPrepared(connection, "update_url", 1, parameters, 0, 0, 0);
		if (PQresultStatus(link_result) != PGRES_COMMAND_OK) {
			printf(" ! Unable to update old URLs in database!\n");
		}

		PQclear(link_result);
		return;
	}
	PQclear(link_result);

	// New URL we need to insert

	parameters[0] = in->nick;
	parameters[1] = url;
	link_result = PQexecPrepared(connection, "create_url", 2, parameters, 0, 0, 0);
	if (PQresultStatus(link_result) != PGRES_COMMAND_OK) {
		printf(" ! Unable to insert new URL in database!\n");
	}
	PQclear(link_result);
}

void log_eightball(const char *nick, const char *query, const char *answer)
{
	const char *parameters[3];
	parameters[0] = nick;
	parameters[1] = query;
	parameters[2] = answer;

	// INSERT INTO eightball_logs(nick_id,query,answer,created_at) VALUES ($1, $2, $3, NOW())
	PGresult *logresult = PQexecPrepared(connection, "log_eightball", 3, parameters, 0, 0, 0);
	if (PQresultStatus(logresult) != PGRES_COMMAND_OK) {
		printf(" ! Unable to log eightball to database!\n");
	}

	PQclear(logresult);
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
