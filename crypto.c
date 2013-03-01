#include "crypto.h"

#include "utf8.h"

#include <stdlib.h>

// Move illegal chars (127-160) into reserved area (734-767)
int fix_illegal(int num, int bool)
{
	if (bool) {
		if (num >= 127 && num <= 160)
			return 734 + (num - 127);
		else
			return num;
	} else {
		if (num >= 734 && num <= 767)
			return 127 + (num - 734);
		else
			return num;
	}
}

// Make number fall within [33,733] (734-767 reserved for illegal characters).
int make_real(int num)
{
	if (num >= 33 && num <= 733)
		return num;
	else if (num < 33)
		return make_real(733 - (32 - num));
	else
		return make_real(num - 701);
}

// Basically a caesar cipher
char *encrypt_subst(char *input)
{
	const int length = strlen(input);
	uint32_t plaintext[length];
	uint32_t key[length];
	int keypos = 0;
	uint32_t output[length*2]; // TODO: fixme
	int outputpos = 0;

	u8_toucs(plaintext, length, input, length);

	int direction = rand() % 2;
	int num = fix_illegal(make_real(33 + (rand() % 400)), 1);
	int num2 = fix_illegal(make_real(33 + (rand() % 400)), 1);

	for (int i=0; i<length; i++) {
		if (plaintext[i] == 32) {
			key[keypos] = fix_illegal(make_real(i - keypos + 33), 1);
			keypos++;
		} else {
			if (direction) {
				output[outputpos++] = fix_illegal(make_real(plaintext[i] + num), 1);
			} else {
				output[outputpos++] = fix_illegal(make_real(plaintext[i] + num), 1);
			}
		}
	}

	output[outputpos++] = 32;
	for (int i=0; i<keypos; i++)
		output[outputpos++] = key[i];
	output[outputpos++] = fix_illegal(make_real(direction + num - num2), 1);
	output[outputpos++] = num;
	output[outputpos++] = num2;

	free(input);

	char *ret = (char*)malloc(length*2); // TODO FIXME
	u8_toutf8(ret, length*2, output, outputpos);
	return ret;
}

// Regular row transposition
char *encrypt_transp(char *input)
{
	return input;
}
/* vim: set ts=8 sw=8 tw=0 noexpandtab cindent softtabstop=8 :*/
