#include "eightball.h"

#include "common.h"
#include "irc.h"

#include <alloca.h>
#include <stdio.h>
#include <string.h>

void eightball(struct recv_data *in, char *arguments)
{
	int count = 0;
	char **args = (char**)alloca(BUFFER);
	char *pch = strtok(arguments, ":");
	while (pch != NULL) {
		args[count] = pch;
		count++;
		pch = strtok(NULL, ":");
	}
	int win = rand() % count;
	char *buf = (char*)malloc(BUFFER);
	sprintf(buf, "PRIVMSG %s :%s: the answer to your question is: %s\n",
			in->channel, in->nick, args[win]);
	send_str(buf);
	free(buf);
}
