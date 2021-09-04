#include "client.h"

#include "drawing.h"
#include "packets.h"
#include "utils.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
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

typedef enum
{
	ScreenRoomSelection,
	ScreenChat
} Screen;

typedef struct {
	int socket_fd;
	pthread_mutex_t socket_lock;
	Screen screen;
	Config *config;
	uint16_t room_index;
} Context;

/*
 * Buffer for chat message which is yet to be sent.
 */
typedef struct {
	unsigned int size;
	unsigned int cursor_pos;
	char *msg;
} ChatBuffer;

static int configure_terminal(int signum);
static void resize_terminal_handler();
static int setup_chat_ui(Context *context);
static int setup_room_selection_ui(Context *context);
static int select_room_keyboard_handler(Context *context, int ch);
static void chat_keyboard_handler(Context *context, ChatBuffer *chat_buffer, int ch);
static void *keyboard_handler(void *arg);
static int set_chat_message(const char *msg);
static int send_chat_message(Context *context, ChatMessage *msg);
static int config_handler(Context *context);
static int handle_heartbeat(Context *context);

static int select_room_keyboard_handler(Context *context, int ch) {
	switch (ch) {
		case INPUT_UP:
			if (context->room_index == 0) {
				break;
			}

			context->room_index--;

			setup_room_selection_ui(context);

			break;

		case INPUT_DOWN:
			if (context->room_index == context->config->num_rooms - 1) {
				break;
			}
			context->room_index++;

			setup_room_selection_ui(context);

			break;

		case INPUT_LINE_FEED:
			if (context->room_index < context->config->num_rooms) {
				if (setup_chat_ui(context) < 0) {
					log_error(ERROR_TERMINAL, "failed to setup UI");

					return -1;
				}
			}

			break;

		default:
			break;
	}

	return 0;
}

static void chat_keyboard_handler(Context *context, ChatBuffer *chat_buffer, int ch) {
	switch (ch) {
		case INPUT_NULL:
			if (set_chat_message(chat_buffer->msg) < 0) {
				log_error(ERROR_TERMINAL, "failed to set chat message");
			}

			printf("\033[%luG", chat_buffer->cursor_pos + CHAT_COL_START);

			break;

		case INPUT_TAB:
			// TODO: change speaker video
			break;

		case INPUT_HOME:
		case INPUT_HOME_2:
			chat_buffer->cursor_pos = 0;
			printf("\033[%luG", CHAT_COL_START);

			break;

		case INPUT_END:
		case INPUT_END_2:
			chat_buffer->cursor_pos = strlen(chat_buffer->msg);
			printf("\033[%luG", chat_buffer->cursor_pos + CHAT_COL_START);

			break;

		case INPUT_ALT_LEFT:
			// If we are on a space or the start of a word, go back to the previous word.
			while (chat_buffer->cursor_pos > 0 && (isspace(chat_buffer->msg[chat_buffer->cursor_pos]) ||
			                                       isspace(chat_buffer->msg[chat_buffer->cursor_pos - 1]))) {
				chat_buffer->cursor_pos--;

				printf("%s", ANSI_CMD_CURSOR_LEFT);
			}

			// Now go to the start of the word.
			while (chat_buffer->cursor_pos > 0 && !isspace(chat_buffer->msg[chat_buffer->cursor_pos - 1])) {
				chat_buffer->cursor_pos--;

				printf("%s", ANSI_CMD_CURSOR_LEFT);
			}

			break;

		case INPUT_ALT_RIGHT:
			// If we are on a space or the end of a word, go forward to the next word.
			while (chat_buffer->cursor_pos < strlen(chat_buffer->msg) &&
			       (isspace(chat_buffer->msg[chat_buffer->cursor_pos]) ||
			        isspace(chat_buffer->msg[chat_buffer->cursor_pos + 1]))) {
				chat_buffer->cursor_pos++;

				printf("%s", ANSI_CMD_CURSOR_RIGHT);
			}

			// Now go to the end of the word.
			while (chat_buffer->cursor_pos < strlen(chat_buffer->msg) &&
			       !isspace(chat_buffer->msg[chat_buffer->cursor_pos])) {
				chat_buffer->cursor_pos++;

				printf("%s", ANSI_CMD_CURSOR_RIGHT);
			}

			break;

		case INPUT_LEFT:
			if (chat_buffer->cursor_pos <= 0) {
				break;
			}

			chat_buffer->cursor_pos--;

			printf("%s", ANSI_CMD_CURSOR_LEFT);

			break;

		case INPUT_RIGHT:
			if (chat_buffer->cursor_pos >= strlen(chat_buffer->msg)) {
				break;
			}

			chat_buffer->cursor_pos++;

			printf("%s", ANSI_CMD_CURSOR_RIGHT);

			break;

		case INPUT_LINE_FEED: {
			if (send_chat_message(context, chat_buffer->msg) < 0) {
				log_error(ERROR_NETWORK, "failed to send message");
			}

			chat_buffer->msg[0] = '\0';
			chat_buffer->cursor_pos = 0;

			printf("\033[%luG%s", CHAT_COL_START, ANSI_CMD_CLEAR_LINE);

			break;
		}

		case INPUT_BACKSPACE:
			if (chat_buffer->cursor_pos == 0) {
				break;
			}

			chat_buffer->cursor_pos--;

			memmove(chat_buffer->msg + chat_buffer->cursor_pos,
			        chat_buffer->msg + chat_buffer->cursor_pos + 1,
			        chat_buffer->size - chat_buffer->cursor_pos - 1);

			if (set_chat_message(chat_buffer->msg) < 0) {
				log_error(ERROR_TERMINAL, "failed to set chat message");
			}

			printf("\033[%luG", chat_buffer->cursor_pos + CHAT_COL_START);

			break;

		case INPUT_DELETE:
		case INPUT_CTRL_D:
			memmove(chat_buffer->msg + chat_buffer->cursor_pos,
			        chat_buffer->msg + chat_buffer->cursor_pos + 1,
			        chat_buffer->size - chat_buffer->cursor_pos - 1);

			if (set_chat_message(chat_buffer->msg) < 0) {
				log_error(ERROR_TERMINAL, "failed to set chat message");
			}

			printf("\033[%luG", chat_buffer->cursor_pos + CHAT_COL_START);

			break;

		default:
			if (isprint(ch) && strlen(chat_buffer->msg) + 1 < chat_buffer->size) {
				memmove(chat_buffer->msg + chat_buffer->cursor_pos + 1,
				        chat_buffer->msg + chat_buffer->cursor_pos,
				        chat_buffer->size - chat_buffer->cursor_pos - 1);
				chat_buffer->msg[chat_buffer->cursor_pos] = (char)ch;
				chat_buffer->cursor_pos++;

				if (set_chat_message(chat_buffer->msg) < 0) {
					log_error(ERROR_TERMINAL, "failed to set chat message");
				}

				printf("\033[%luG", chat_buffer->cursor_pos + CHAT_COL_START);
			}
	}
}

static void *keyboard_handler(void *arg) {
	Context *context = (Context *)arg;
	ChatBuffer chat_buffer = {0};

	while (TRUE) {
		int ch = 0;

		if (read(STDIN_FILENO, &ch, sizeof ch) <= 0) {
			break;
		}

		if (ch == INPUT_ESCAPE) {
			break;
		}

		struct winsize window_size;

		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) < 0) {
			log_error(ERROR_TERMINAL, "failed to get terminal size");

			return NULL;
		}

		if (window_size.ws_col < MIN_WINDOW_WIDTH || window_size.ws_row < MIN_WINDOW_HEIGHT) {
			continue;
		} else if (chat_buffer.size != (unsigned int)window_size.ws_col - 5) {
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

		switch (context->screen) {
			case ScreenRoomSelection:
				select_room_keyboard_handler(context, ch);

				break;

			case ScreenChat:
				chat_keyboard_handler(context, &chat_buffer, ch);

				break;

			default:
				break;
		}

		fflush(stdout);
	}

	if (shutdown(context->socket_fd, SHUT_RDWR) < 0) {
		log_error(ERROR_NETWORK, "failed to disconnect from server");
	}

	return NULL;
}

static int set_chat_message(const char *msg) {
	struct winsize window_size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) < 0) {
		log_error(ERROR_TERMINAL, "failed to get terminal size");

		return -1;
	}

	printf("\033[%u;%uH\033[K%s %s", window_size.ws_row, 1, CHAT_PROMPT, msg);
	fflush(stdout);

	return 0;
}

static int send_chat_message(Context *context, ChatMessage *msg) {
	log_info("sending message");

	Serialised *serialised = serialise_chat_message(msg);

	if (send_packet(context->socket_fd, serialised, &context->socket_lock) < 0) {
		log_error(ERROR_NETWORK, "failed to send chat message");

		free(serialised);

		return -1;
	}

	free(serialised);

	return 0;
}

// FIXME: resize terminal before closing results in no final newline
static void resize_terminal_handler() {
	log_info("received terminal resize signal");

	struct winsize window_size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) < 0) {
		log_error(ERROR_TERMINAL, "failed to get terminal size");
	}

	if (window_size.ws_col < MIN_WINDOW_WIDTH || window_size.ws_row < MIN_WINDOW_HEIGHT) {
		printf("%s\033[?25h", ANSI_CMD_CLEAR_SCREEN);
		printf("\033[HTerminal must have at least:\n"
		       "\t* %d columns\n"
		       "\t* %d rows\n"
		       "Please resize to continue.\n",
		       MIN_WINDOW_WIDTH,
		       MIN_WINDOW_HEIGHT);

		fflush(stdout);
	} else {
		char null = INPUT_NULL;

		if (ioctl(STDIN_FILENO, TIOCSTI, &null) < 0) {
			log_error(ERROR_TERMINAL, "failed to send fake (null) input trigger");
		}
	}
}

/*
 * Draw room selection UI on client terminal.
 */
static int setup_room_selection_ui(Context *context) {
	struct winsize window_size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) < 0) {
		log_error(ERROR_TERMINAL, "failed to get terminal size");

		return -1;
	}

	printf("%s%s", ANSI_CMD_CLEAR_SCREEN, ANSI_CMD_CURSOR_RESET);
	printf("%s %s - %s\n\n", APP_NAME, APP_VERSION, APP_DESC);
	printf("Select room:\n\n");
	printf("\033[%dG\033[1mName", ROOM_LIST_LEFT_MARGIN + 1);
	printf("\033[%dGParticipants", ROOM_LIST_LEFT_MARGIN + ROOM_LIST_COLUMN_NAME_WIDTH + 1);
	printf("\033[%dGDescription\033[0m\n",
	       ROOM_LIST_LEFT_MARGIN + ROOM_LIST_COLUMN_NAME_WIDTH + ROOM_LIST_COLUMN_PARTICIPANTS_WIDTH + 1);

	draw_line(ROOM_LIST_LEFT_MARGIN,
	          CURSOR_CURRENT_POS,
	          LineTypeHorizontal,
	          window_size.ws_col - ROOM_LIST_LEFT_MARGIN - ROOM_LIST_RIGHT_MARGIN,
	          CHAR_HORIZONTAL_LINE);

	printf("\n\033[s");

	for (size_t i = 0; i < context->config->num_rooms; i++) {
		if (i == context->room_index) {
			printf("\033[%dG\u27a4 \033[7m", ROOM_LIST_LEFT_MARGIN - 1);
			print_multi(" ", window_size.ws_col - ROOM_LIST_RIGHT_MARGIN - ROOM_LIST_RIGHT_MARGIN);
		}

		printf("\033[%dG%s", ROOM_LIST_LEFT_MARGIN + 1, context->config->rooms[i].name);
		// TODO: participants
		printf("\033[%dG0", ROOM_LIST_LEFT_MARGIN + ROOM_LIST_COLUMN_NAME_WIDTH + 1);
		printf("\033[%dG\033[3m%s\033[0m\n",
		       ROOM_LIST_LEFT_MARGIN + ROOM_LIST_COLUMN_NAME_WIDTH + ROOM_LIST_COLUMN_PARTICIPANTS_WIDTH + 1,
		       context->config->rooms[i].desc);

		if (i == context->room_index) {
			printf("\033[0m");
		}
	}

	printf("\033[u\033[%dG\033[?25l", ROOM_LIST_LEFT_MARGIN - 1);
	fflush(stdout);

	context->screen = ScreenRoomSelection;

	return 0;
}

/*
 * Draw chat UI on client terminal.
 */
static int setup_chat_ui(Context *context) {
	struct winsize window_size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) < 0) {
		log_error(ERROR_TERMINAL, "failed to get terminal size");

		return -1;
	}

	printf("%s\033[?25h", ANSI_CMD_CLEAR_SCREEN);

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

	if (set_chat_message("") < 0) {
		log_error(ERROR_TERMINAL, "failed to reset chat message");

		return -1;
	}

	context->screen = ScreenChat;

	return 0;
}

/*
 * Sets up or resets the user terminal.
 * if signum <= 0: setup terminal
 * if signum > 0: reset to previous settings
 */
static int configure_terminal(int signum) {
	static struct termios old_term = {0};
	static int configured = FALSE;

	if (signum < 0) {
		log_info("setting up terminal");

		if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
			log_error(ERROR_TERMINAL, "failed to get terminal attributes");

			return -1;
		}

		configured = TRUE;
		struct termios new_term = old_term;
		new_term.c_lflag &= ~ECHO & ~ICANON;

		if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) < 0) {
			log_error(ERROR_TERMINAL, "failed to set terminal attributes");

			return -1;
		}

		printf("%s", ANSI_CMD_ENABLE_ALTERNATE_BUFFER);
		fflush(stdout);
	} else if (configured) {
		log_info("resetting terminal");

		if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) < 0) {
			log_error(ERROR_TERMINAL, "failed to set terminal attributes");

			return -1;
		}

		printf("%s\033[?25h", ANSI_CMD_DISABLE_ALTERNATE_BUFFER);
		fflush(stdout);
	} else {
		log_info("terminal not configured, no need for resetting");
	}

	return 0;
}

static int config_handler(Context *context) {
	Serialised serialised = {0};
	int ret = recv_packet(context->socket_fd, &serialised, &context->socket_lock);

	if (ret < 0) {
		log_error(ERROR_NETWORK, "failed to receive configuration");

		return ret;
	}

	free(context->config);

	context->config = unserialise_config(&serialised);
	context->room_index = 0;

	return setup_room_selection_ui(context);
}

int handle_heartbeat(Context *context) {
	Serialised serialised = {0};
	int ret = recv_packet(context->socket_fd, &serialised, &context->socket_lock);

	if (ret < 0) {
		log_error(ERROR_NETWORK, "failed to receive heartbeat ping");

		return ret;
	} else if (ret == 0) {
		return 0;
	}

	if (unserialise_heartbeat(&serialised) != HeartbeatPing) {
		log_error(ERROR_NETWORK, "heartbeat from server was not a ping... this is awkward...");

		return -1;
	}

	Heartbeat heartbeat = HeartbeatPong;
	Serialised *send_serialised = serialise_heartbeat(heartbeat);

	if (send_packet(context->socket_fd, send_serialised, &context->socket_lock) < 0) {
		free(send_serialised);

		log_error(ERROR_NETWORK, "failed to send pong");

		return -1;
	}

	free(send_serialised);

	return 0;
}

int main() {
	draw_init();

	struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(5000)};

	if (inet_pton(AF_INET, "localhost", &server_addr.sin_addr) < 0) {
		log_fatal(ERROR_NETWORK, "failed to parse IP address");
	}

	Context context = {.socket_fd = socket(AF_INET, SOCK_STREAM, 0), .socket_lock = PTHREAD_MUTEX_INITIALIZER};

	if (context.socket_fd < 0) {
		log_fatal(ERROR_NETWORK, "failed to construct socket");
	}

	if (connect(context.socket_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
		log_fatal(ERROR_NETWORK, "failed to connect to server");
	}

	// FIXME: we need to catch SIGTERM and SIGINT to avoid abort()
	// but we don't want to do anything with it because it will interrupt send/recv
	// anyway, which results in clean shutdown anyway.... hmmm....
	struct sigaction reset_action = {.sa_handler = do_nothing /*(void (*)(int))configure_terminal*/};

	if (sigaction(SIGINT, &reset_action, NULL) < 0) {
		log_error(ERROR_TERMINAL, "failed to set SIGINT terminal reset signal");
	}

	if (sigaction(SIGTERM, &reset_action, NULL) < 0) {
		log_error(ERROR_TERMINAL, "failed to set SIGTERM terminal reset signal");
	}

	struct sigaction resize_action = {.sa_flags = SA_RESTART, .sa_handler = resize_terminal_handler};

	if (sigaction(SIGWINCH, &resize_action, NULL) < 0) {
		log_error(ERROR_TERMINAL, "failed to set SIGWINCH terminal resize signal");
	}

	if (setup_terminal() < 0) {
		log_fatal(ERROR_TERMINAL, "failed to setup terminal");
	}

	pthread_t keyboard_thread;

	if (pthread_create(&keyboard_thread, NULL, keyboard_handler, &context) != 0) {
		log_fatal(ERROR_THREAD, "failed to start keyboard listening thread");
	}

	while (TRUE) {
		PacketType packet_type;
		int n = 0;

		if ((n = recv(context.socket_fd, &packet_type, sizeof packet_type, MSG_PEEK)) == 0) {
			log_info("server disconnected");

			break;
		} else if (n < 0) {
			// If EINTR received something like SIGINT was received.
			if (errno != EINTR) {
				log_error(ERROR_NETWORK, "failed to receive packet type");
			}

			break;
		}

		switch (packet_type) {
			case PacketTypeHeartbeat: {
				handle_heartbeat(&context);

				break;
			}

			case PacketTypeConfig: {
				log_info("received config");

				config_handler(&context);

				break;
			}

			default:;
		}
	}

	if (close(context.socket_fd) < 0) {
		log_error(ERROR_NETWORK, "failed to disconnect from server");
	}

	free(context.config);

	if (reset_terminal() < 0) {
		log_error(ERROR_TERMINAL, "failed to reset terminal");
	}

	return EXIT_SUCCESS;
}
