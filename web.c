#include "web.h"

#include "common.h"
#include "irc.h"
#include "entities.h"

#include <curl/curl.h>
#include <string.h>
#include <pcre.h>

#define HTTP_BUFFER 1024000 //100kb http buffer

char http_buffer[HTTP_BUFFER];
size_t http_buffer_pos;

size_t http_write_callback(void *contents, size_t element_size, size_t num_elements, void *userpointer)
{
	(void)userpointer; // We don't need this from curl

	size_t size = element_size * num_elements;

	if (size + http_buffer_pos > HTTP_BUFFER) {
		size = HTTP_BUFFER - http_buffer_pos;
	}

	memcpy(&http_buffer[http_buffer_pos], contents, size);
	http_buffer_pos += size;
	return size;
}

void clean_spaces(char *str)
{
	unsigned int i = 0, j = 0, stringlength = strlen(str);

	while (i < stringlength)
	{
		// Change tabs into spaces
		if (str[i] == '\t')
		{
			str[i] = ' ';
			// Fallthrough
		}

		// Skip subsequent whitespaces
		if (str[i] == ' ' && i < stringlength-1 && str[i+1] == ' ')
		{
			i++;
			continue;
		}

		// Skip leading whitespace
		if (str[i] == ' ' && j == 0)
		{
			i++;
			continue;
		}

		// Skip trailing whitespace
		if (str[i] == ' ' && i == stringlength-1)
		{
			i++;
			continue;
		}

		str[j] = str[i];
		i++;
		j++;
	}
	str[j] = '\0';
}

void strip_newlines(char *str)
{
	for (unsigned int i=0; i<strlen(str); i++) if (str[i] == '\n' || str[i] == '\r') str[i] = ' ';
}

#define ANGRY_ROBOT "\342\224\227\133\302\251\040\342\231\222\040\302\251\135\342\224\233\040\357\270\265\040\342\224\273\342\224\201\342\224\273"
char *fetch_url_and_match(const char *url, const pcre *pattern)
{
	http_buffer_pos = 0;

	int curl_err;
	char err_buf[256];
	char *result = calloc(sizeof(char), BUFFER_SIZE);
	CURL *curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, http_write_callback);
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, err_buf);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_err = curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);

	if (curl_err != 0) {
		// Error 23 means that the HTTP_BUFFER is full, can be ignored
		if (curl_err != 23) {
			printf("CURL ERROR: %d (%s)\n", curl_err, err_buf);
			strcpy(result, ANGRY_ROBOT);
			return result;
		}
	}

	int res_buffers[30];
	int resultcount = pcre_exec(pattern, 0, http_buffer, http_buffer_pos, 0, PCRE_NO_UTF8_CHECK, res_buffers, 30);
	if (resultcount > 0) {
		pcre_copy_substring(http_buffer, res_buffers, resultcount, 1, result, BUFFER_SIZE);
		strip_newlines(result);
		clean_spaces(result);
		decode_html_entities_utf8(result, NULL);
	} else {
		result[0] = '\0'; // empty string
	}
	return result;
}

void get_title_from_url(struct recv_data *in, const char *url)
{
	char *title = fetch_url_and_match(url, patterns->html_title);
	if (strcmp(title, "")) {
		char buf[strlen(in->channel) + strlen(title) + 15];
		sprintf(buf, "PRIVMSG %s :>> %s\n", in->channel, title);
		irc_send_str(buf);
	} else {
		printf(" ! got empty title\n");
	}
	free(title);
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
