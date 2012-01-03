#include "title.h"

#include "main.h"

#include <string.h>
#include <curl/curl.h>

#define HTTP_BUFFER 10240 //10kb http buffer
char http_buffer[HTTP_BUFFER];
size_t http_buffer_pos;

size_t http_write_callback(void *contents, size_t element_size, size_t num_elements, void *userpointer)
{
	size_t size = element_size * num_elements;

	if (size + http_buffer_pos > HTTP_BUFFER) {
		size = HTTP_BUFFER - http_buffer_pos;
	}

	if (size < 0) {
		return 0;
	}

	memcpy(&http_buffer[http_buffer_pos], contents, size);
	http_buffer_pos += size;
	return element_size * num_elements;
}

void strip_newlines(char *str)
{
	for (int i=0; i<strlen(str); i++) if (str[i] == '\n') str[i] = ' ';
}

void check_for_url(struct recv_data *in)
{
	const char *msg = in->message;
	int offsets[30];
	int offsetcount = pcre_exec(patterns->url, 0, msg, strlen(msg), 0, 0, offsets, 30);
	if (offsetcount > 0) {
		for (int i=1; i<offsetcount; i++) {
			char url[BUFFER];
			pcre_copy_substring(msg, offsets, offsetcount, i, url, BUFFER);
			printf("Got url: %s\n", url);

			http_buffer_pos = 0;
			CURL *curl_handle = curl_easy_init();
			curl_easy_setopt(curl_handle, CURLOPT_URL, url);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &http_write_callback);
			curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
			//curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
			curl_easy_perform(curl_handle);
			curl_easy_cleanup(curl_handle);

			int titles[30];
			int titlecount = pcre_exec(patterns->html_title, 0, http_buffer, HTTP_BUFFER, 0, 0, titles, 30);
			char title[BUFFER];
			if (titlecount > 0) {
				pcre_copy_substring(http_buffer, titles, titlecount, 1, title, BUFFER);
				strip_newlines(title);
				printf("%s\n", title);
				char *buf = malloc(strlen(title) + strlen(in->nick) + 10 + 4);
				sprintf(buf, "PRIVMSG %s :>> %s\n", in->channel, title);
				send_str(buf);
				free(buf);
			}
		}
	}
}


