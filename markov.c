#include "markov.h"
#include "irc.h"

#define _GNU_SOURCE 1
#include <fcntl.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memset
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct hash_item {
	int words_count;

	int allocated_words;

	char **words;

	char *word;

	struct hash_item *next;
} hash_item;

hash_item hash_map[HASHTABLE_SIZE];
int initialized = 0;

// djb2
unsigned long hash(const char *string)
{
	unsigned long hash = 5381;
	unsigned int character;
	while ((character = *string++)) {
		hash = hash * 33 ^ character;
	}

	return hash % HASHTABLE_SIZE;
}

hash_item *get_item(hash_item table[], const char *word1, const char *word2, int create_new)
{
	char tuple[strlen(word1) + strlen(word2) + 1]; // + 1 for null terminating byte
	strcpy(tuple, word1);
	strcat(tuple, word2);

	hash_item *item = &table[hash(tuple)];

	if (!item->word) {
		item->word = strdup(tuple);
		return item;
	}

	while (1) {
		if (!strcmp(tuple, item->word)) {
			return item;
		}
		if (!item->next) {
			if (create_new) {
				item->next = malloc(sizeof(hash_item));
				item = item->next;
				item->word = strdup(tuple);
				item->words = 0;
				item->words_count = 0;
				item->allocated_words = 0;
				item->next = 0;
				return item;
			} else {
				return 0;
			}
		}

		item = item->next;
	}
}

void free_item(hash_item *item)
{
	for (int j=0; j < item->words_count; j++) {
		free(item->words[j]);
	}
	free(item->word);
	free(item->words);
}

void markov_terminate()
{
	initialized = 0;
	for (size_t i=0; i<HASHTABLE_SIZE; i++) {
		hash_item *item = &hash_map[i];
		free_item(item);
		item = item->next;
		while (item) {
			free_item(item);
			hash_item *parent = item;
			item = parent->next;
			free(parent);
		}
	}
}

void markov_init(const char *corpus_file)
{
	FILE *file = fopen(corpus_file, "r");
	if (file == NULL) {
		printf(" ! markov: unable to open input file\n");
		return;
	}

	fseek(file, 0L, SEEK_END);
	size_t length = ftell(file);
	fseek(file, 0L, SEEK_SET);

	char *text = malloc(length+1000);
	if (!text) {
		printf(" ! markov: unable to allocate memory for corpus file\n");
		fclose(file);
		return;
	}

	size_t read_bytes = fread(text, sizeof(unsigned char), length, file);
	if (length != read_bytes) {
		printf(" ! markov: unable to read entire file\n");
		fclose(file);
		free(text);
		return;
	}
	fclose(file);

	text[length] = 0;
	// Strip newlines
	for (size_t i = 0; i < length; i++) {
		if (text[i]=='\n') {
			text[i]=' ';
		}
	}

	srand(time(NULL));

	memset(hash_map, 0, sizeof(hash_map));

	char *word1 = strtok(text, " ");
	char *word2 = strtok(NULL, " ");
	char *word3 = strtok(NULL, " ");
	int pos = 0;
	while (word3) {
		hash_item *item = get_item(hash_map, word1, word2, 1);
		item->words_count++;
		if (item->words_count > item->allocated_words) {
			item->allocated_words += 20;
			size_t new_size = item->allocated_words * sizeof(char*);
			item->words = realloc(item->words, new_size);
		}

		item->words[item->words_count-1] = strdup(word3);
		if (pos % 10000 == 0 && length) {
			printf(" + markov: %ld%% parsing complete\n", 100 * ((size_t)word3 - (size_t)text) / length);
		}

		word1 = word2;
		word2 = word3;
		word3 = strtok(NULL, " ");
		++pos;
	}
	free(text);
	printf(" ! markov: %d trigrams parsed\n", pos);
	for (size_t i=0; i<HASHTABLE_SIZE; i++) {
		hash_item *item = &hash_map[i];
		item->words = realloc(item->words, item->words_count * sizeof(char*));
		item = item->next;
		while (item) {
			item->words = realloc(item->words, item->words_count * sizeof(char*));
			item = item->next;
		}
	}
	malloc_trim(0);
	printf(" ! markov: pruned tree\n");
	initialized = 1;
}

char *add_word(char *string, const char *word)
{
	string = realloc(string, strlen(string) + strlen(word) + 2);
	string = strcat(string, word);
	string = strcat(string, " ");
	return string;
}

#define TRIGGER "!apropos "

void markov_parse(struct recv_data *in)
{
	if (strncmp(in->message, TRIGGER, strlen(TRIGGER)) != 0) {
		return;
	}
	if (strlen(in->message) < strlen(TRIGGER) + 5) {
		return;
	}
	if (!initialized) {
		return;
	}

	char *input = strdup(in->message + strlen(TRIGGER));
	strip_newlines(input);
	char *previous = strtok(input, " ");
	strip_newlines(previous);
	char *word;
	hash_item *item = NULL;
	while ((word = strtok(NULL, " "))) {
		strip_newlines(word);
		item = get_item(hash_map, previous, word, 0);
		if (item) break;
		previous = word;
	}
	if (!item) {
		free(input);
		return; // break early if no recognized tuplets
	}

	char *buf = malloc(strlen("PRIVMSG  :") + strlen(in->channel));
	sprintf(buf, "PRIVMSG %s :", in->channel);
	buf = add_word(buf, previous);
	buf = add_word(buf, word);

	while (item && item->words_count) {
		char *next_word = item->words[random() % item->words_count];
		buf = add_word(buf, next_word);
		if (next_word[strlen(next_word)-1] == '.') break;
		if (next_word[strlen(next_word)-1] == '\n') break;
		if (next_word[strlen(next_word)-1] == '\r') break;
		if (next_word[strlen(next_word)-1] == ',') break;
		if (next_word[strlen(next_word)-1] == '!') break;
		item = get_item(hash_map, word, next_word, 0);
		word = next_word;
	}

	clean_spaces(buf);
	if (strlen(buf) > 1) {
		buf[strlen(buf)-1] = 0; // strip terminating character
	}
	strip_newlines(buf);
	buf = add_word(buf, "\n");

	irc_send_str(buf);
	free(buf);
	free(input);

	return;
}

/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
