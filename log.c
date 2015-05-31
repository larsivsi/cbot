#include "log.h"

#include "config.h"
#include "common.h"
#include "irc.h"

#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>

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

	result = PQprepare(connection, "update_nick", "UPDATE nicks SET updated_at=NOW(), words=words+$1, identity = $3 WHERE nick=$2", 0, 0);
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

	result = PQprepare(connection, "get_nick_identity", "SELECT identity FROM nicks WHERE nick=$1", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (get_nick_identity)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	// URL queries
	result = PQprepare(connection, "create_url", "INSERT INTO links(nick_id, link, created_at, times_posted) VALUES ((SELECT id FROM nicks WHERE nick = $1), $2, NOW(), 1)", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (create_url): %s\n", PQresultErrorMessage(result));
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
		PQclear(result);
		printf(" ! Unable to create prepared function (log_message): %s\n", PQresultErrorMessage(result));
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

	result = PQprepare(connection, "check_op", "SELECT COUNT(*) FROM operators WHERE ident = $1 AND channel = $2", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (check_op)!\n");
		log_abort();
		return;
	}
	PQclear(result);

	result = PQprepare(connection, "add_op", "INSERT INTO operators(ident, channel) VALUES($1, $2)", 0, 0);
	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		PQclear(result);
		printf(" ! Unable to create prepared function (add_op)!\n");
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
	for (unsigned int i=0; i<strlen(string); i++) {
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

	const char *parameters[3];
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

	// Update nick info
	char *numwords_buf = malloc(sizeof(int)*8+1);
	sprintf(numwords_buf, "%d", count_words(in->message));
	parameters[0] = numwords_buf;

	parameters[1] = in->nick;

	parameters[2] = in->ident;

	wordResult = PQexecPrepared(connection, "update_nick", 3, parameters, 0, 0, 0);
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
		irc_send_str(buf);
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

char *log_get_identity(const char *nick)
{
	PGresult *result;

	const char *parameters[1];
	parameters[0] = nick;
	result = PQexecPrepared(connection, "get_nick_identity", 1, parameters, 0, 0, 0);
	if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
		printf(" ! Unable to get identity from database!\n");
		return 0;
	}

	char *identity = PQgetvalue(result, 0, 0);
	if (strcmp(identity, "") == 0) {
		identity = 0;
	} else {
		// original return value cleared when we run PQclear, so we copy
		char *buffer = malloc(strlen(identity));
		strcpy(buffer, identity);
		identity = buffer;
	}
	PQclear(result);
	return identity;
}

void db_user_add_op(const char *nick, const char *channel)
{
	char *ident = log_get_identity(nick);
	if (!ident) {
		printf(" ! Unable to get identity of nick %s\n", nick);
		return;
	}

	const char *parameters[2];
	parameters[0] = ident;
	parameters[1] = channel;

	PGresult *addresult = PQexecPrepared(connection, "add_op", 2, parameters, 0, 0, 0);
	if (PQresultStatus(addresult) != PGRES_COMMAND_OK) {
		printf(" ! Unable to add operator to database!\n");
	}

	PQclear(addresult);
}

int db_user_is_op(const char *ident, const char *channel)
{
	PGresult *result;

	const char *parameters[2];
	parameters[0] = ident;
	parameters[1] = channel;
	result = PQexecPrepared(connection, "check_op", 2, parameters, 0, 0, 0);
	if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
		printf(" ! Unable to check if user %s is op in db: %s\n", ident, PQerrorMessage(connection));
		return 0;
	}

	int count = atoi(PQgetvalue(result, 0, 0));
	PQclear(result);
	return count;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
