/*
 * Copyright (C) 2014 Julien Bonjean <julien@bonjean.info>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "config.h"

#define NORMAL  "\x1B[0m"
#define RED  "\x1B[31m"
#define GREEN  "\x1B[32m"

#define XSTR(A) STR(A)
#define STR(A) #A

#define MAX_INPUT_LEN 100
#define MAX_LINE_LEN 512
#define MAX_HISTORY_SIZE (1024 * 256)

#ifdef DEBUG
#define debug(fmt, ...) \
	fprintf(stderr, "DEBUG %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define debug(fmt, ...) \
	do { } while (0)
#endif /* DEBUG */

#define error(fmt, ...) \
	fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#ifndef PROMPT
#define PROMPT(buffer, saved, direction, index, result) \
	do { \
		/* print the first part of the prompt */ \
		fprintf(stderr, "%s<%s search> %s", saved, direction, buffer); \
		if (index > 0) { \
			/* save cursor position */ \
			fprintf(stderr, "\033[s"); \
			/* if there is a result, append it */ \
			fprintf(stderr, " (%d)[%s]", index, result); \
			/* restore cursor position */ \
			fprintf(stderr, "\033[u"); \
		}} while (0)
#endif /* PROMPT */

#ifdef BASH
	#define MIN_CMD_LEN 3
#else
	#define CMD_PREFIX "- cmd: "
	#define CMD_PREFIX_LEN 7
	#define MIN_CMD_LEN 10
#endif /* BASH */

typedef enum {
	SEARCH_BACKWARD, SEARCH_FORWARD
} action_t;

/* will be used as exit code to differenciate cases */
typedef enum {
	RESULT_EXECUTE = 0, SEARCH_CANCEL = 1, RESULT_EDIT = 10
} exit_t;

struct termios saved_attributes;
char *history[MAX_HISTORY_SIZE];
char buffer[MAX_INPUT_LEN];
char saved[128];
unsigned long history_size;
int search_result_index;

void reset_input_mode() {
	debug("restore terminal settings");

	tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

int set_input_mode() {
	debug("setup terminal");

	struct termios tattr;

	// make sure stdin is a terminal
	if (!isatty(STDIN_FILENO)) {
		error("not a terminal");
		return 1;
	}

	// save the terminal attributes so we can restore them later
	tcgetattr(STDIN_FILENO, &saved_attributes);
	atexit(reset_input_mode);

	// set the funny terminal modes
	tcgetattr(STDIN_FILENO, &tattr);
	tattr.c_lflag &= ~(ICANON | ECHO);
	tattr.c_cc[VMIN] = 1;
	tattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);

	return 0;
}

FILE *try_open_history(const char *name) {
	char path[1024];
	snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), name);
	return fopen(path, "r");
}

int parse_history() {
	debug("parse history");

	FILE* fp;
	char cmdline[MAX_LINE_LEN];
	int i,j,len;
#ifdef CHECK_DUPLICATES
	int k,dup;
#endif

#ifdef BASH
	fp = try_open_history(".bash_history");
#else
	fp = try_open_history(".local/share/fish/fish_history");
	if (!fp) fp = try_open_history(".config/fish/fish_history");
#endif
	if (!fp) {
		error("cannot open history file");
		return 1;
	}

	// parse the history file
	while (fgets(cmdline, sizeof(cmdline), fp) != NULL) {

		// skip if truncated
		len = strlen(cmdline);
		if (len == 0 || cmdline[len - 1] != '\n')
			continue;
		cmdline[--len] = 0; // remove \n

		// skip if too short
		if (len < MIN_CMD_LEN)
			continue;
#ifndef BASH
		// skip if pattern not matched
		if (strncmp(CMD_PREFIX, cmdline, CMD_PREFIX_LEN) != 0)
			continue;

		// sanitize
		i = CMD_PREFIX_LEN; j = 0;
		while (i < len) {
			if (i < (len - 1) && cmdline[i] == '\\' && cmdline[i+1] == '\\') {
				i++;
			}
			cmdline[j] = cmdline[i];
			i++; j++;
		}
		cmdline[j] = '\0';
		len = j;
#endif

		// append to history array
		history[history_size] = malloc(len + 1);
		if (!history[history_size]) {
			error("cannot allocate memory");
			fclose(fp);
			return 1;
		}
		strncpy(history[history_size], cmdline, len + 1);
		history_size++;

		if (history_size >= MAX_HISTORY_SIZE) {
			error("too many history entries");
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);

#ifdef CHECK_DUPLICATES
	// check for duplicates, it is easier to do it afterward to preserve
	// history order
	k = 0;
	for (i = 0 ; i < history_size ; i++) {
		dup = 0;
		for (j = i + 1 ; j < history_size ; j++) {
			if (!strcmp(history[i], history[j])) {
				dup = 1;
				break;
			}
		}
		if (dup) {
			free(history[i]);
		} else {
			if (i != k)
				history[k] = history[i];
			k++;
		}
	}
	// adjust history size
	history_size = k;
#endif

	debug("%d entries loaded", history_size);
	return 0;
}

void restore_terminal() {
	// reset color
	fprintf(stderr, "%s", NORMAL);

	// clear last printed results
	fprintf(stderr, "\33[2K\r");

	// restore line wrapping
	fprintf(stderr, "\033[?7;h");

	fflush(stderr);
}

int nb_getchar() {
	int c;

	// mark stdin nonblocking
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	c = getchar();

	// restore stdin
	fcntl(STDIN_FILENO, F_SETFL, flags);

	return c;
}

void free_history() {
	int i = 0;
	for (i = 0; i < history_size; i++)
		free(history[i]);

	// just to be ensure we won't free again
	history_size = 0;
}

void cleanup() {
	debug("cleanup resources");

	restore_terminal();
	free_history();
}

void accept(int status) {
	// print result/buffer to stdout
	fprintf(stdout, "%s",
			search_result_index < history_size ?
					history[search_result_index] : buffer);
	cleanup();
	exit(status);
}

void cancel() {
	cleanup();
	exit(SEARCH_CANCEL);
}

int main() {
	// ensure everything is initialized
	buffer[0] = '\0';
	history_size = 0;

	// handle sigint for clean exit
	signal(SIGINT, cancel);

	// prepare terminal
	if (set_input_mode())
		exit(EXIT_FAILURE);

	// load history
	if (parse_history()) {
		free_history();
		exit(EXIT_FAILURE);
	}

	char c;

	int search_index = 0;
	int buffer_pos = 0;
	int i, j;
	action_t action = SEARCH_BACKWARD;
	search_result_index = history_size;

	// if the buffer environment variable is set, populate the input buffer
	char* env_buffer = getenv("SEARCH_BUFFER");
	if (env_buffer && strlen(env_buffer) > 0) {
		strncpy(buffer, env_buffer, sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = '\0';
		buffer_pos = strlen(buffer);

		// remove trailing '\n'
		if (buffer[buffer_pos - 1] == '\n')
			buffer[--buffer_pos] = '\0';
	}

	// disable line wrapping
	fprintf(stderr, "\033[?7l");

	while (1) {
		if (buffer_pos > 0 || strlen(saved) > 0) {
			// search in the history array
			// TODO: factorize?
			if (action == SEARCH_BACKWARD) {
				for (i = search_result_index - 1; i >= 0; i--) {
					if (strstr(history[i], buffer)) {
						search_index++;
						search_result_index = i;
						break;
					}
				}
			} else {
				for (i = search_result_index + 1; i < history_size; i++) {
					if (strstr(history[i], buffer)) {
						search_index--;
						search_result_index = i;
						break;
					}
				}
			}
		}

		// erase line
		fprintf(stderr, "\033[2K\r");

		// print the color
		fprintf(stderr, "%s", search_index > 0 ? GREEN : RED);

		// print the prompt
		PROMPT(buffer, saved, (action == SEARCH_BACKWARD ? "backward" : "forward"),
				search_index,
				search_index > 0 ? history[search_result_index] : "");

		fflush(stderr);

		c = getchar();

		switch (c) {
		case 27:
			c = nb_getchar();
			if (c == -1) { // esc
				cancel();
				break;
			} else if (c != 91 && c != 79) {
				ungetc(c, stdin);
				break;
			}

			// multi-characters sequence
			switch (getchar()) {
			case 53: // pg-up
				getchar();
				/* no break */
			case 65: // up
				action = SEARCH_BACKWARD;
				break;
			case 54: // pg-down
				getchar();
				/* no break */
			case 66: // down
				action = SEARCH_FORWARD;
				break;
			case 70: // end
			case 67: // right
				accept(RESULT_EDIT);
				break;
			case 72: // home
			case 68: // left
				cancel();
				break;
			}
			break;

		case 4: // C-d
			cancel();
			break;

		case 5: // C-e
			accept(RESULT_EDIT);
			break;

		case 10: // enter
		case 13: // new-line
			accept(RESULT_EXECUTE);
			break;

		case 18: //C-r
			action = SEARCH_BACKWARD;
			break;

		case 19: //C-s
			action = SEARCH_FORWARD;
			break;

		case 21: //C-u
			if (strlen(buffer) == 0)
				break;
			j = 0;
			// filter the history array to remove non-matching entries
			for (i = 0; i < history_size; i++) {
				if (strstr(history[i], buffer) && i != j) {
					history[j] = history[i];
					// update the search result index
					if (search_result_index == i)
						search_result_index = j;
					j++;
				} else
					free(history[i]);
			}
			// adjust history size
			history_size = j;
			// add the saved search keyword
			strncat(saved, "[", sizeof(saved) - 1);
			strncat(saved, buffer, sizeof(saved) - 1);
			strncat(saved, "]", sizeof(saved) - 1);
			// reset the buffer
			buffer[0] = '\0';
			buffer_pos = 0;
			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;
			break;

		case 127: // backspace
		case 8: // backspace
			if (buffer_pos > 0)
				buffer[--buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;

			break;

		default:
			// exclude the first 32 non-printing characters
			if (c < 32)
				break;

			// prevent buffer overflow
			if (buffer_pos >= MAX_INPUT_LEN - 1)
				continue;

			buffer[buffer_pos] = c;
			buffer[++buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;
		}
	}

	error("should not happen");
	cancel();
}
