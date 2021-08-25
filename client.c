#include "client.h"

#include "packets.h"
#include "utils.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

void *handle_keyboard() {
	struct termios new_term, old_term;

	if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
		log_fatal("Could not get terminal attributes.");
	}

	if (tcgetattr(STDIN_FILENO, &new_term) < 0) {
		log_fatal("Could not get terminal attributes.");
	}

	new_term.c_lflag &= ~ECHO & ~ICANON;

	struct winsize window_size;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);

	int escape = 0;
	ChatBuffer buffer = {0};
	buffer.size = window_size.ws_col - 5;
	buffer.msg = calloc(1, buffer.size);

	while (TRUE) {
		tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
		int ch = getchar();

		if (ch == '\t') {
			ch = ' ';
		} else if (ch == CHAR_HOME) {
			buffer.cursor_pos = 0;
			printf("\033[%dG", CHAT_COL_START);

			continue;
		} else if (ch == CHAR_END) {
			buffer.cursor_pos = strlen(buffer.msg);
			printf("\033[%dG", buffer.cursor_pos + CHAT_COL_START);

			continue;
		} else if (ch == CHAR_ESCAPE) {
			escape = CHAR_ESCAPE;

			continue;
		} else if (escape == CHAR_ESCAPE && ch == CHAR_ESCAPE_FUNCTION) {
			escape = CHAR_ESCAPE_FUNCTION;

			continue;
		} else if (escape == CHAR_ESCAPE && ch == CHAR_ESCAPE_ALT_LEFT) {
			// If we are on a space or the start of a word, go back to the previous word.
			while (buffer.cursor_pos > 0 &&
			       (isspace(buffer.msg[buffer.cursor_pos]) || isspace(buffer.msg[buffer.cursor_pos - 1]))) {
				buffer.cursor_pos--;

				printf("\033[1D");
			}

			// Now go to the start of the word.
			while (buffer.cursor_pos > 0 && !isspace(buffer.msg[buffer.cursor_pos - 1])) {
				buffer.cursor_pos--;

				printf("\033[1D");
			}

			continue;
		} else if (escape == CHAR_ESCAPE && ch == CHAR_ESCAPE_ALT_RIGHT) {
			// If we are on a space or the end of a word, go forward to the next word.
			while (buffer.cursor_pos < strlen(buffer.msg) &&
			       (isspace(buffer.msg[buffer.cursor_pos]) || isspace(buffer.msg[buffer.cursor_pos + 1]))) {
				buffer.cursor_pos++;

				printf("\033[1C");
			}

			// Now go to the end of the word.
			while (buffer.cursor_pos < strlen(buffer.msg) && !isspace(buffer.msg[buffer.cursor_pos])) {
				buffer.cursor_pos++;

				printf("\033[1C");
			}

			continue;
		} else if (escape == CHAR_ESCAPE_FUNCTION) {
			escape = 0;

			switch (ch) {
				case CHAR_ESCAPE_LEFT:
					if (buffer.cursor_pos <= 0) {
						break;
					}

					buffer.cursor_pos--;

					printf("\033[1D");

					break;

				case CHAR_ESCAPE_RIGHT:
					if (buffer.cursor_pos >= strlen(buffer.msg)) {
						break;
					}

					buffer.cursor_pos++;

					printf("\033[1C");

					break;

				case CHAR_ESCAPE_HOME:
					buffer.cursor_pos = 0;
					printf("\033[%dG", CHAT_COL_START);

					break;

				case CHAR_ESCAPE_END:
					buffer.cursor_pos = strlen(buffer.msg);
					printf("\033[%dG", buffer.cursor_pos + CHAT_COL_START);

					break;
			}

			continue;
		} else if (ch == '\n') {
			continue;
		} else if (ch == 127) {
			if (buffer.cursor_pos == 0) {
				continue;
			}

			buffer.cursor_pos--;

			memmove(buffer.msg + buffer.cursor_pos,
			        buffer.msg + buffer.cursor_pos + 1,
			        buffer.size - buffer.cursor_pos - 1);

			printf("\033[%dG", CHAT_COL_START);
			printf("\033[0K");
			printf("%s", buffer.msg);
			printf("\033[%dG", buffer.cursor_pos + CHAT_COL_START);

			continue;
		} else if (ch == 4) {
			memmove(buffer.msg + buffer.cursor_pos,
			        buffer.msg + buffer.cursor_pos + 1,
			        buffer.size - buffer.cursor_pos - 1);

			printf("\033[%dG", CHAT_COL_START);
			printf("\033[0K");
			printf("%s", buffer.msg);
			printf("\033[%dG", buffer.cursor_pos + CHAT_COL_START);

			continue;
		}

		if (strlen(buffer.msg) + 1 < buffer.size) {
			memmove(buffer.msg + buffer.cursor_pos + 1,
			        buffer.msg + buffer.cursor_pos,
			        buffer.size - buffer.cursor_pos - 1);
			buffer.msg[buffer.cursor_pos] = (char)ch;
			buffer.cursor_pos++;

			printf("\033[%dG", CHAT_COL_START);
			printf("\033[0K");
			printf("%s", buffer.msg);
			printf("\033[%dG", buffer.cursor_pos + CHAT_COL_START);
		}

		tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
	}
}

void setup_ui() {
	struct winsize window_size;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);

	fprintf(stdout, "\033[2J");

	for (int row_num = 1; row_num < window_size.ws_row - 1; row_num++) {
	}

	for (int col_num = 1; col_num <= window_size.ws_col; col_num++) {
		if (col_num == window_size.ws_col - 16) {
			printf("\033[%u;%uH\u253b", window_size.ws_row - 1, window_size.ws_col - 16);
		} else {
			printf("\033[%u;%uH\u2501", window_size.ws_row - 1, col_num);
		}
	}

	printf("\033[%u;%uH%s", window_size.ws_row, 1, CHAT_PROMPT);
	fflush(stdout);
}

int main() {
	struct sockaddr_in server = {0};
	server.sin_family = AF_INET;
	inet_pton(AF_INET, "localhost", &server.sin_addr);
	server.sin_port = htons(5000);

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (socket_fd < 0) {
		log_fatal("Could not create socket.");
	}

	if (connect(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		log_fatal("Could not connect to server.");
	}

	setup_ui();

	pthread_t keyboard_thread;

	pthread_create(&keyboard_thread, NULL, handle_keyboard, NULL);

	while (TRUE) {
		Heartbeat heartbeat;
		int n = recv(socket_fd, &heartbeat, sizeof(heartbeat), 0);

		if (n <= 0) {
			break;
		}

		// printf("recv: %d\n", n);

		if (heartbeat.status == HeartbeatStatusPing) {
			PacketType packet_type = PacketTypeHeartbeat;
			heartbeat.status = HeartbeatStatusPong;

			// printf("ponging\n");

			send(socket_fd, &packet_type, sizeof(packet_type), 0);
			send(socket_fd, &heartbeat, sizeof(heartbeat), 0);
		}
	}

	return EXIT_SUCCESS;
}
