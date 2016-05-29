#include "eightball.h"

#include "common.h"
#include "irc.h"
#include "log.h"

#include <alloca.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void eightball(struct recv_data *in, char *arguments)
{
	int count = 0;
	char **args = (char**)alloca(BUFFER_SIZE);
	char *pch = strtok(arguments, ":");
	while (pch != NULL) {
		args[count] = pch;
		count++;
		pch = strtok(NULL, ":");
	}
	if (count == 0) {
		printf("No options given!\n");
		return;
	}
	int win = rand() % count;
	char *buf = (char*)malloc(BUFFER_SIZE);
	sprintf(buf, "PRIVMSG %s :%s: the answer to your question is: %s\n",
			in->channel, in->nick, args[win]);
	irc_send_str(buf);
	free(buf);

	log_eightball(in->nick, arguments, args[win]);
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
