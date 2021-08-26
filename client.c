#include "client.h"

#include "packets.h"
#include "utils.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define setup_terminal() configure_terminal(-1)
#define reset_terminal() configure_terminal(0)

typedef struct {
	unsigned int size;
	unsigned int cursor_pos;
	char *msg;
} ChatBuffer;

static void configure_terminal(int signum);
static void resize_terminal();
static int setup_ui();
static void *handle_keyboard(void *arg);
static void send_chat_message(int socket_fd, ChatMessage *msg);

static void *handle_keyboard(void *arg) {
	int socket_fd = *(int *)arg;
	ChatBuffer chat_buffer = {0};

	while (TRUE) {
		int ch = 0;
		read(STDIN_FILENO, &ch, sizeof(int));
		struct winsize window_size;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);

		if (window_size.ws_col < MIN_WINDOW_WIDTH || window_size.ws_row < MIN_WINDOW_HEIGHT) {
			continue;
		} else if (chat_buffer.size != window_size.ws_col - 5) {
			chat_buffer.msg = realloc(chat_buffer.msg, window_size.ws_col - 5);
			unsigned int new_size = window_size.ws_col - 5;

			if (chat_buffer.size == 0) {
				chat_buffer.msg[0] = '\0';
			} else if (new_size < chat_buffer.size) {
				chat_buffer.msg[new_size - 1] = '\0';
			}

			if (chat_buffer.cursor_pos >= new_size - 1) {
				chat_buffer.cursor_pos = new_size - 2;
			}

			chat_buffer.size = new_size;
		}

		switch (ch) {
			case INPUT_NULL:
				printf("\033[%luG", CHAT_COL_START);
				printf("\033[0K");
				printf("%s", chat_buffer.msg);
				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			case INPUT_ESCAPE:
				goto exit_loop;

			case INPUT_TAB:
				// TODO: change speaker video
				break;

			case INPUT_HOME:
				chat_buffer.cursor_pos = 0;
				printf("\033[%luG", CHAT_COL_START);

				break;

			case INPUT_END:
				chat_buffer.cursor_pos = strlen(chat_buffer.msg);
				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			case INPUT_ALT_LEFT:
				// If we are on a space or the start of a word, go back to the previous word.
				while (chat_buffer.cursor_pos > 0 && (isspace(chat_buffer.msg[chat_buffer.cursor_pos]) ||
				                                      isspace(chat_buffer.msg[chat_buffer.cursor_pos - 1]))) {
					chat_buffer.cursor_pos--;

					printf("\033[1D");
				}

				// Now go to the start of the word.
				while (chat_buffer.cursor_pos > 0 && !isspace(chat_buffer.msg[chat_buffer.cursor_pos - 1])) {
					chat_buffer.cursor_pos--;

					printf("\033[1D");
				}

				break;

			case INPUT_ALT_RIGHT:
				// If we are on a space or the end of a word, go forward to the next word.
				while (chat_buffer.cursor_pos < strlen(chat_buffer.msg) &&
				       (isspace(chat_buffer.msg[chat_buffer.cursor_pos]) ||
				        isspace(chat_buffer.msg[chat_buffer.cursor_pos + 1]))) {
					chat_buffer.cursor_pos++;

					printf("\033[1C");
				}

				// Now go to the end of the word.
				while (chat_buffer.cursor_pos < strlen(chat_buffer.msg) &&
				       !isspace(chat_buffer.msg[chat_buffer.cursor_pos])) {
					chat_buffer.cursor_pos++;

					printf("\033[1C");
				}

				break;

			case INPUT_LEFT:
				if (chat_buffer.cursor_pos <= 0) {
					break;
				}

				chat_buffer.cursor_pos--;

				printf("\033[1D");

				break;

			case INPUT_RIGHT:
				if (chat_buffer.cursor_pos >= strlen(chat_buffer.msg)) {
					break;
				}

				chat_buffer.cursor_pos++;

				printf("\033[1C");

				break;

			case INPUT_LINE_FEED: {
				ChatMessage msg = {.size = strlen(chat_buffer.msg) + 1, .msg = chat_buffer.msg};

				send_chat_message(socket_fd, &msg);

				chat_buffer.msg[0] = '\0';
				chat_buffer.cursor_pos = 0;

				printf("\033[%luG", CHAT_COL_START);
				printf("\033[0K");

				break;
			}

			case INPUT_BACKSPACE:
				if (chat_buffer.cursor_pos == 0) {
					continue;
				}

				chat_buffer.cursor_pos--;

				memmove(chat_buffer.msg + chat_buffer.cursor_pos,
				        chat_buffer.msg + chat_buffer.cursor_pos + 1,
				        chat_buffer.size - chat_buffer.cursor_pos - 1);

				printf("\033[%luG", CHAT_COL_START);
				printf("\033[0K");
				printf("%s", chat_buffer.msg);
				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			case INPUT_DELETE:
				memmove(chat_buffer.msg + chat_buffer.cursor_pos,
				        chat_buffer.msg + chat_buffer.cursor_pos + 1,
				        chat_buffer.size - chat_buffer.cursor_pos - 1);

				printf("\033[%luG", CHAT_COL_START);
				printf("\033[0K");
				printf("%s", chat_buffer.msg);
				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			default:
				if (strlen(chat_buffer.msg) + 1 < chat_buffer.size) {
					memmove(chat_buffer.msg + chat_buffer.cursor_pos + 1,
					        chat_buffer.msg + chat_buffer.cursor_pos,
					        chat_buffer.size - chat_buffer.cursor_pos - 1);
					chat_buffer.msg[chat_buffer.cursor_pos] = (char)ch;
					chat_buffer.cursor_pos++;

					printf("\033[%luG", CHAT_COL_START);
					printf("\033[0K");
					printf("%s", chat_buffer.msg);
					printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);
				}
		}

		fflush(stdout);
	}

exit_loop:
	close(socket_fd);

	return NULL;
}

static void send_chat_message(int socket_fd, ChatMessage *msg) {
	PacketType packet_type = PacketTypeChatMessage;

	send(socket_fd, &packet_type, sizeof(packet_type), 0);
	send(socket_fd, &msg->size, sizeof(msg->size), 0);
	send(socket_fd, msg->msg, strlen(msg->msg) + 1, 0);
}

// FIXME: resize terminal before closing results in no final newline
static void resize_terminal() {
	if (setup_ui() == 0) {
		char null = INPUT_NULL;
		ioctl(STDIN_FILENO, TIOCSTI, &null);
	}
}

/*
 * Draw UI on client terminal.
 */
static int setup_ui() {
	struct winsize window_size;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);

	printf("\033[2J");

	if (window_size.ws_col < MIN_WINDOW_WIDTH || window_size.ws_row < MIN_WINDOW_HEIGHT) {
		printf("\033[HTerminal must have at least:\n"
		       "\t* %d columns\n"
		       "\t* %d rows\n"
		       "Please resize to continue.\n",
		       MIN_WINDOW_WIDTH,
		       MIN_WINDOW_HEIGHT);

		fflush(stdout);

		return -1;
	}

	for (int row_num = 1; row_num < window_size.ws_row - 1; row_num++) {
		printf("\033[%u;%uH\u2503", row_num, window_size.ws_col - CHAT_BOX_WIDTH);
	}

	for (int col_num = 1; col_num <= window_size.ws_col; col_num++) {
		if (col_num > window_size.ws_col - CHAT_BOX_WIDTH) {
			printf("\033[%u;%uH\u2501", 1, col_num);
			printf("\033[%u;%uH\u2501", 10, col_num);
		}

		if (col_num == window_size.ws_col - CHAT_BOX_WIDTH) {
			printf("\033[%u;%uH\u2523", 1, col_num);
			printf("\033[%u;%uH\u2523", 10, col_num);
			printf("\033[%u;%uH\u253b", window_size.ws_row - 1, col_num);
		} else {
			printf("\033[%u;%uH\u2501", window_size.ws_row - 1, col_num);
		}
	}

	printf("\033[%u;%luH %s ",
	       1,
	       window_size.ws_col - (CHAT_BOX_WIDTH / 2) - (strlen(PARTICIPANTS_TITLE) / 2),
	       PARTICIPANTS_TITLE);

	printf("\033[%u;%luH %s ", 10, window_size.ws_col - (CHAT_BOX_WIDTH / 2) - (strlen(CHAT_TITLE) / 2), CHAT_TITLE);

	printf("\033[%u;%uH%s ", window_size.ws_row, 1, CHAT_PROMPT);
	fflush(stdout);

	return 0;
}

/*
 * Sets up or resets the user terminal.
 * if signum <= 0: setup terminal
 * if signum > 0: reset to previous settings
 */
static void configure_terminal(int signum) {
	static struct termios old_term = {0};

	if (signum < 0) {
		if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
			log_fatal("Could not get terminal attributes.");
		}

		struct termios new_term = {0};
		memcpy(&new_term, &old_term, sizeof(old_term));
		new_term.c_lflag &= ~ECHO & ~ICANON;
		tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

		printf("\033[?1049h");
		fflush(stdout);

		setup_ui();
	} else {
		tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

		printf("\033[?1049l");
		fflush(stdout);
	}
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

	struct sigaction reset_action = {.sa_handler = configure_terminal};
	sigaction(SIGINT, &reset_action, NULL);
	sigaction(SIGTERM, &reset_action, NULL);

	struct sigaction resize_action = {.sa_flags = SA_RESTART, .sa_handler = resize_terminal};
	sigaction(SIGWINCH, &resize_action, NULL);

	setup_terminal();

	pthread_t keyboard_thread;

	pthread_create(&keyboard_thread, NULL, handle_keyboard, &socket_fd);

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

	reset_terminal();

	return EXIT_SUCCESS;
}
