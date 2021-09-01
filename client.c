#include "client.h"

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

typedef struct {
	int socket_fd;
	pthread_mutex_t socket_lock;
} Server;

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
static int setup_ui();
static void *keyboard_handler(void *arg);
static int set_chat_message(const char *msg);
static int send_chat_message(Server *server, ChatMessage *msg);
static int config_handler(Server *server);
static int handle_heartbeat(Server *server);

static void *keyboard_handler(void *arg) {
	Server *server = (Server *)arg;
	ChatBuffer chat_buffer = {0};

	while (TRUE) {
		int ch = 0;

		if (read(STDIN_FILENO, &ch, sizeof ch) <= 0) {
			break;
		}

		// fprintf(stderr, "%d ", ch);
		// continue;
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

		switch (ch) {
			case INPUT_NULL:
				if (set_chat_message(chat_buffer.msg) < 0) {
					log_error(ERROR_TERMINAL, "failed to set chat message");
				}

				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			case INPUT_ESCAPE:
				if (close(server->socket_fd) < 0) {
					log_error(ERROR_NETWORK, "failed to disconnect from server");
				}

				return NULL;

			case INPUT_TAB:
				// TODO: change speaker video
				break;

			case INPUT_HOME:
			case INPUT_HOME_2:
				chat_buffer.cursor_pos = 0;
				printf("\033[%luG", CHAT_COL_START);

				break;

			case INPUT_END:
			case INPUT_END_2:
				chat_buffer.cursor_pos = strlen(chat_buffer.msg);
				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			case INPUT_ALT_LEFT:
				// If we are on a space or the start of a word, go back to the previous word.
				while (chat_buffer.cursor_pos > 0 && (isspace(chat_buffer.msg[chat_buffer.cursor_pos]) ||
				                                      isspace(chat_buffer.msg[chat_buffer.cursor_pos - 1]))) {
					chat_buffer.cursor_pos--;

					printf("%s", ANSI_CMD_CURSOR_LEFT);
				}

				// Now go to the start of the word.
				while (chat_buffer.cursor_pos > 0 && !isspace(chat_buffer.msg[chat_buffer.cursor_pos - 1])) {
					chat_buffer.cursor_pos--;

					printf("%s", ANSI_CMD_CURSOR_LEFT);
				}

				break;

			case INPUT_ALT_RIGHT:
				// If we are on a space or the end of a word, go forward to the next word.
				while (chat_buffer.cursor_pos < strlen(chat_buffer.msg) &&
				       (isspace(chat_buffer.msg[chat_buffer.cursor_pos]) ||
				        isspace(chat_buffer.msg[chat_buffer.cursor_pos + 1]))) {
					chat_buffer.cursor_pos++;

					printf("%s", ANSI_CMD_CURSOR_RIGHT);
				}

				// Now go to the end of the word.
				while (chat_buffer.cursor_pos < strlen(chat_buffer.msg) &&
				       !isspace(chat_buffer.msg[chat_buffer.cursor_pos])) {
					chat_buffer.cursor_pos++;

					printf("%s", ANSI_CMD_CURSOR_RIGHT);
				}

				break;

			case INPUT_LEFT:
				if (chat_buffer.cursor_pos <= 0) {
					break;
				}

				chat_buffer.cursor_pos--;

				printf("%s", ANSI_CMD_CURSOR_LEFT);

				break;

			case INPUT_RIGHT:
				if (chat_buffer.cursor_pos >= strlen(chat_buffer.msg)) {
					break;
				}

				chat_buffer.cursor_pos++;

				printf("%s", ANSI_CMD_CURSOR_RIGHT);

				break;

			case INPUT_LINE_FEED: {
				if (send_chat_message(server, chat_buffer.msg) < 0) {
					log_error(ERROR_NETWORK, "failed to send message");
				}

				chat_buffer.msg[0] = '\0';
				chat_buffer.cursor_pos = 0;

				printf("\033[%luG%s", CHAT_COL_START, ANSI_CMD_CLEAR_LINE);

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

				if (set_chat_message(chat_buffer.msg) < 0) {
					log_error(ERROR_TERMINAL, "failed to set chat message");
				}

				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			case INPUT_DELETE:
			case INPUT_CTRL_D:
				memmove(chat_buffer.msg + chat_buffer.cursor_pos,
				        chat_buffer.msg + chat_buffer.cursor_pos + 1,
				        chat_buffer.size - chat_buffer.cursor_pos - 1);

				if (set_chat_message(chat_buffer.msg) < 0) {
					log_error(ERROR_TERMINAL, "failed to set chat message");
				}

				printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);

				break;

			default:
				if (isprint(ch) && strlen(chat_buffer.msg) + 1 < chat_buffer.size) {
					memmove(chat_buffer.msg + chat_buffer.cursor_pos + 1,
					        chat_buffer.msg + chat_buffer.cursor_pos,
					        chat_buffer.size - chat_buffer.cursor_pos - 1);
					chat_buffer.msg[chat_buffer.cursor_pos] = (char)ch;
					chat_buffer.cursor_pos++;

					if (set_chat_message(chat_buffer.msg) < 0) {
						log_error(ERROR_TERMINAL, "failed to set chat message");
					}

					printf("\033[%luG", chat_buffer.cursor_pos + CHAT_COL_START);
				}
		}

		fflush(stdout);
	}

	if (close(server->socket_fd) < 0) {
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

static int send_chat_message(Server *server, ChatMessage *msg) {
	log_info("sending message");

	Serialised *serialised = serialise_chat_message(msg);

	if (send_packet(server->socket_fd, serialised, &server->socket_lock) < 0) {
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

	switch (setup_ui()) {
		case 0: {
			char null = INPUT_NULL;

			if (ioctl(STDIN_FILENO, TIOCSTI, &null) < 0) {
				log_error(ERROR_TERMINAL, "failed to send fake (null) input trigger");
			}

			break;
		}

		// Terminal too small.
		case 1:
			break;

		default:
			log_error(ERROR_TERMINAL, "failed to setup UI");
	}
}

/*
 * Draw UI on client terminal.
 */
static int setup_ui() {
	struct winsize window_size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) < 0) {
		log_error(ERROR_TERMINAL, "failed to get terminal size");

		return -1;
	}

	printf("%s", ANSI_CMD_CLEAR_SCREEN);

	if (window_size.ws_col < MIN_WINDOW_WIDTH || window_size.ws_row < MIN_WINDOW_HEIGHT) {
		printf("\033[HTerminal must have at least:\n"
		       "\t* %d columns\n"
		       "\t* %d rows\n"
		       "Please resize to continue.\n",
		       MIN_WINDOW_WIDTH,
		       MIN_WINDOW_HEIGHT);

		fflush(stdout);

		return 1;
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

	if (set_chat_message("") < 0) {
		log_error(ERROR_TERMINAL, "failed to reset chat message");

		return -1;
	}

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
		struct termios new_term = {0};
		memcpy(&new_term, &old_term, sizeof old_term);
		new_term.c_lflag &= ~ECHO & ~ICANON;

		if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) < 0) {
			log_error(ERROR_TERMINAL, "failed to set terminal attributes");

			return -1;
		}

		printf("%s", ANSI_CMD_ENABLE_ALTERNATE_BUFFER);

		if (setup_ui() < 0) {
			log_error(ERROR_TERMINAL, "failed to setup UI");

			return -1;
		}
	} else if (configured) {
		log_info("resetting terminal");

		if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) < 0) {
			log_error(ERROR_TERMINAL, "failed to set terminal attributes");

			return -1;
		}

		printf("%s", ANSI_CMD_DISABLE_ALTERNATE_BUFFER);
		fflush(stdout);
	} else {
		log_info("terminal not configured, no need for resetting");
	}

	return 0;
}

static int config_handler(Server *server) {
	Serialised serialised = {0};
	int ret = recv_packet(server->socket_fd, &serialised, &server->socket_lock);

	if (ret < 0) {
		log_error(ERROR_NETWORK, "failed to receive configuration");

		return ret;
	}

	Config *config = unserialise_config(&serialised);

	printf("%s%s", ANSI_CMD_CLEAR_SCREEN, ANSI_CMD_CURSOR_RESET);
	printf("Select room:\n");

	for (size_t i = 0; i < config->num_rooms; i++) {
		printf("%s - %s\n", config->rooms[i].name, config->rooms[i].desc);
	}

	fflush(stdout);

	free(config);

	return 0;
}

int handle_heartbeat(Server *server) {
	Serialised serialised = {0};
	int ret = recv_packet(server->socket_fd, &serialised, &server->socket_lock);

	if (ret < 0) {
		log_error(ERROR_NETWORK, "failed to receive heartbeat ping");

		return ret;
	} else if (ret == 0) {
		return 0;
	}

	Heartbeat *heartbeat = unserialise_heartbeat(&serialised);

	if (*heartbeat != HeartbeatStatusPing) {
		log_error(ERROR_NETWORK, "heartbeat from server was not a ping... this is awkward...");

		return -1;
	}

	*heartbeat = HeartbeatStatusPong;
	Serialised *send_serialised = serialise_heartbeat(heartbeat);

	free(heartbeat);

	if (send_packet(server->socket_fd, send_serialised, &server->socket_lock) < 0) {
		free(send_serialised);

		log_error(ERROR_NETWORK, "failed to send pong");

		return -1;
	}

	free(send_serialised);

	return 0;
}

int main() {
	struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(5000)};

	if (inet_pton(AF_INET, "localhost", &server_addr.sin_addr) < 0) {
		log_fatal(ERROR_NETWORK, "failed to parse IP address");
	}

	Server server = {.socket_fd = socket(AF_INET, SOCK_STREAM, 0), .socket_lock = PTHREAD_MUTEX_INITIALIZER};

	if (server.socket_fd < 0) {
		log_fatal(ERROR_NETWORK, "failed to construct socket");
	}

	if (connect(server.socket_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
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

	if (pthread_create(&keyboard_thread, NULL, keyboard_handler, &server) != 0) {
		log_fatal(ERROR_THREAD, "failed to start keyboard listening thread");
	}

	while (TRUE) {
		PacketType packet_type;
		int n = 0;

		if ((n = recv(server.socket_fd, &packet_type, sizeof packet_type, MSG_PEEK)) == 0) {
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
				handle_heartbeat(&server);

				break;
			}

			case PacketTypeConfig: {
				log_info("received config");

				config_handler(&server);

				break;
			}

			default:;
		}
	}

	if (close(server.socket_fd) < 0) {
		log_error(ERROR_NETWORK, "failed to disconnect from server");
	}

	if (reset_terminal() < 0) {
		log_error(ERROR_TERMINAL, "failed to reset terminal");
	}

	return EXIT_SUCCESS;
}
