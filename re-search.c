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

#define NORMAL  "\x1B[0m"
#define RED  "\x1B[31m"
#define GREEN  "\x1B[32m"

#define XSTR(A) STR(A)
#define STR(A) #A

#define MAX_INPUT_LEN 100
#define MAX_LINE_LEN 512
#define MAX_HISTORY_SIZE 8192

#ifdef DEBUG
#define debug(fmt, ...) \
	fprintf(stderr, "DEBUG %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define debug(fmt, ...) \
	do { } while (0)
#endif

#define error(fmt, ...) \
	fprintf(stderr, fmt "\n", ##__VA_ARGS__)

typedef enum {
	SEARCH_BACKWARD, SEARCH_FORWARD
} action_t;

struct termios saved_attributes;
char *history[MAX_HISTORY_SIZE];
char buffer[MAX_INPUT_LEN];
int history_size;
int search_result_index;

void reset_input_mode(void) {
	debug("restore terminal settings");

	tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

int set_input_mode(void) {
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

int parse_fish_history() {
	debug("parse fish history");

	FILE* fp;
	int res;
	char path[1024];
	char cmdline[MAX_LINE_LEN + 1];

	snprintf(path, sizeof(path), "%s/.config/fish/fish_history",
			getenv("HOME"));

	fp = fopen(path, "r");
	if (!fp) {
		error("cannot open history file");
		return 1;
	}

	// parse the fish history file and search for line with pattern "- cmd: *"
	while ((res = fscanf(fp, "- cmd: %" XSTR(MAX_LINE_LEN) "[^\n]\n", cmdline))
			!= EOF) {
		if (res == 1) {
			// append to history array, we don't check for duplicates as it
			// should be handled by the shell
			history[history_size] = malloc(strlen(cmdline) + 1);
			if (!history[history_size]) {
				error("cannot allocate memory");
				fclose(fp);
				return 1;
			}
			strncpy(history[history_size], cmdline, strlen(cmdline) + 1);
			history_size++;

			if (history_size >= MAX_HISTORY_SIZE) {
				error("too many history entries");
				fclose(fp);
				return 1;
			}
		}
		// we need to consume at least one character for fscan to continue
		// scanning (why?), should we use fseek?
		fgetc(fp);
	}

	fclose(fp);

	debug("%d entries loaded", history_size);
	return 0;
}

void restore_terminal() {
	// reset color
	fprintf(stderr, "%s", NORMAL);

	// clear last printed results
	fprintf(stderr, "\33[2K");

	// restore cursor position
	fprintf(stderr, "\033[u");

	fflush(stderr);
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

void accept() {
	// print result/buffer to stdout
	fprintf(stdout, "%s",
			search_result_index < history_size ?
					history[search_result_index] : buffer);
	cleanup();
	exit(EXIT_SUCCESS);
}

void cancel() {
	cleanup();
	exit(EXIT_FAILURE);
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

	// load fish history
	if (parse_fish_history()) {
		free_history();
		exit(EXIT_FAILURE);
	}

	char c;

	int search_index = 0;
	int buffer_pos = 0;
	int i = 0;
	action_t action = SEARCH_BACKWARD;
	search_result_index = history_size;

	// if the buffer environment variable is set, populate the input buffer
	char* env_buffer = getenv("SEARCH_BUFFER");
	if (env_buffer && strlen(env_buffer) > 0) {
		strncpy(buffer, env_buffer, strlen(env_buffer) + 1);
		buffer_pos = strlen(env_buffer);

		// remove trailing '\n'
		if (buffer[buffer_pos - 1] == '\n')
			buffer[--buffer_pos] = '\0';
	}

	// save cursor position
	fprintf(stderr, "\033[s");

	while (1) {
		if (buffer_pos > 0) {
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

		// print the first part of the prompt
		fprintf(stderr, "%s<%s search> %s", search_index > 0 ? GREEN : RED,
				action == SEARCH_BACKWARD ? "backward" : "forward", buffer);

		// if there is a result, append it
		if (search_index > 0) {
			i = fprintf(stderr, " (%d)[%s]", search_index,
					history[search_result_index]);

			// move back the cursor after the user input
			fprintf(stderr, "\033[%dD", i);
		}

		fflush(stderr);

		c = getchar();

		switch (c) {
		case 27:
			c = getchar();
			if (c == 27) {
				// second escape (we cannot just catch a single escape because
				// of the multi-characters sequences
				cancel();
				break;
			} else if (c != 91) {
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
			case 67: // right
				accept();
				break;
			case 68: // left
				cancel();
				break;
			}
			break;

		case '\004': // C-d
			cancel();
			break;

		case '\n': // Enter
			accept();
			break;

		case 18: //C-r
			action = SEARCH_BACKWARD;
			break;

		case 5: //C-e
			action = SEARCH_FORWARD;
			break;

		case 127: // Backspace
			if (buffer_pos <= 0)
				continue;

			buffer[--buffer_pos] = '\0';

			// reset search
			action = SEARCH_BACKWARD;
			search_result_index = history_size;
			search_index = 0;
			break;

		default:
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
