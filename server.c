#include "server.h"

#include "packets.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static void *heartbeat_handler(void *arg);
static void *client_handler(void *arg);
static Config *read_config();
static int send_config(const Client *client, const Config *config);

static void *heartbeat_handler(void *arg) {
	Client *client = (Client *)arg;
	struct sigaction disconnect_action = {.sa_handler = do_nothing};

	if (sigaction(SIGUSR1, &disconnect_action, NULL) < 0) {
		log_error(ERROR_TERMINAL, "failed to set SIGUSR1 client disconnection signal");
	}

	while (TRUE) {
		client->heartbeat = HeartbeatPing;
		Serialised *serialised = serialise_heartbeat(client->heartbeat);

		if (send_packet(client->socket_fd, serialised, &client->socket_lock) < 0) {
			log_error(ERROR_HEARTBEAT, "failed to send packet type");

			break;
		}

		// Wait for client_handler to receive pong. May be interrupted if told to finish up.
		if (sleep(HEARTBEAT_INTERVAL) != 0) {
			break;
		}

		if (client->heartbeat != HeartbeatPong) {
			pthread_mutex_lock(&client->socket_lock);

			// Check if client has already disconnected.
			if (send(client->socket_fd, NULL, 0, 0) < 0) {
				if (errno == EBADF) {
					pthread_mutex_unlock(&client->socket_lock);

					break;
				} else {
					log_error(ERROR_HEARTBEAT, "failed to check status of client");
				}
			}

			pthread_mutex_unlock(&client->socket_lock);

			log_error(ERROR_HEARTBEAT, "client has not responded to last ping with a pong");

			if (shutdown(client->socket_fd, SHUT_RDWR) < 0) {
				log_error(ERROR_NETWORK, "failed to disconnect from client");
			}

			break;
		}
	}

	return NULL;
}

static Config *read_config() {
	char *home_dir = get_home_dir();

	if (home_dir == NULL) {
		log_error(ERROR_OS, "failed to get home directory");

		return NULL;
	}

	char *config_path = join_path(home_dir, CONFIG_PATH, NULL);
	char *dir = NULL;

	for (size_t i = 1; i < strlen(config_path); i++) {
		if (config_path[i] == PATH_SEPARATOR) {
			if (dir != NULL) {
				free(dir);
			}

			dir = strndup(config_path, i);

			if (mkdir(dir, 0777) < 0 && errno != EEXIST) {
				log_error(ERROR_CONFIG, "failed to create config directory");

				freep(dir);

				return NULL;
			}
		}
	}

	freep(dir);

#ifdef __APPLE__
	int config_fd = open(config_path, O_RDONLY | O_CREAT | O_SYMLINK, 0666);
#else
	int config_fd = open(config_path, O_RDONLY | O_CREAT, 0666);
#endif

	if (config_fd < 0) {
		log_error(ERROR_CONFIG, "failed to create/open configuration file");

		return NULL;
	}

	freep(config_path);

	FILE *config_file = fdopen(config_fd, "r");

	if (config_file == NULL) {
		log_error(ERROR_CONFIG, "failed to open configuration file stream");

		return NULL;
	}

	Config *config = calloc(1, sizeof *config);
	char *line = NULL;
	size_t cap = 0;
	ssize_t n = 0;
	ConfigSection section = ConfigSectionGlobal;

	while ((n = getline(&line, &cap, config_file)) > 0) {
		char *stripped = strip_whitespace(line);

		if (stripped[0] == CONFIG_COMMENT || strlen(stripped) == 0) {
			continue;
		}

		if (stripped[0] == CONFIG_SECTION_START && stripped[strlen(stripped) - 1] == CONFIG_SECTION_END) {
			if (strncasecmp(stripped + 1, "rooms", strlen(stripped) - 2) == 0) {
				section = ConfigSectionRooms;
			}
		} else {
			char *key = stripped;
			char *value = NULL;

			for (size_t i = 0; i < strlen(stripped); i++) {
				if (stripped[i] == '=') {
					stripped[i] = '\0';
					value = &stripped[i + 1];

					break;
				}
			}

			if (section == ConfigSectionRooms) {
				config->rooms = realloc(config->rooms, sizeof *config->rooms * (config->num_rooms + 1));
				config->rooms[config->num_rooms].name = strdup(key);
				config->rooms[config->num_rooms].desc = strdup(value);
				config->num_rooms++;
			}
		}

		freep(stripped);
	}

	freep(line);

	if (ferror(config_file) != 0) {
		log_error(ERROR_CONFIG, "failed to read lines from configuration file");

		if (fclose(config_file) != 0) {
			log_error(ERROR_CONFIG, "failed to close configuration file stream");
		}

		return NULL;
	}

	if (fclose(config_file) != 0) {
		log_error(ERROR_CONFIG, "failed to close configuration file stream");
	}

	return config;
}

static int send_config(const Client *client, const Config *config) {
	Serialised *serialised = serialise_config(config);

	int n = send_packet(client->socket_fd, serialised, (pthread_mutex_t *)&client->socket_lock);

	if (n < 0) {
		log_error(ERROR_NETWORK, "failed to send packet type");

		return -1;
	}

	return 0;
}

static void *client_handler(void *arg) {
	Client client = {.socket_fd = *(int *)arg, .socket_lock = PTHREAD_MUTEX_INITIALIZER};

	log_info("connected to client");

	if (pthread_create(&client.heartbeat_thread, NULL, heartbeat_handler, &client) != 0) {
		log_fatal(ERROR_THREAD, "failed to create client heartbeat thread");
	}

	Config *config = read_config();

	if (config == NULL) {
		log_error(ERROR_CONFIG, "failed to read configuration file");
	}

	if (send_config(&client, config) < 0) {
		log_error(ERROR_CONFIG, "failed to send configuration to client");
	}

	while (TRUE) {
		PacketType packet_type;
		int n = recv(client.socket_fd, &packet_type, sizeof packet_type, MSG_PEEK);

		if (n < 0) {
			log_error(ERROR_NETWORK, "failed to receive packet type");

			break;
		} else if (n == 0) {
			// Instruct the heartbeat handler to finish up.
			if (pthread_kill(client.heartbeat_thread, SIGUSR1) != 0) {
				log_error(ERROR_THREAD, "failed to signal client heartbeat thread to finish");
			}

			if (close(client.socket_fd) < 0) {
				log_error(ERROR_NETWORK, "failed to disconnect from client");
			}

			break;
		}

		if (packet_type == PacketTypeHeartbeat) {
			Serialised serialised = {0};
			int ret = recv_packet(client.socket_fd, &serialised, &client.socket_lock);

			if (ret < 0) {
				log_error(ERROR_NETWORK, "failed to receive heartbeat");

				break;
			} else if (ret == 0) {
				break;
			}

			client.heartbeat = unserialise_heartbeat(&serialised);
		} else if (packet_type == PacketTypeChatMessage) {
			printf("received chat message\n");

			Serialised serialised = {0};
			int ret = recv_packet(client.socket_fd, &serialised, &client.socket_lock);

			if (ret < 0) {
				log_error(ERROR_NETWORK, "failed to receive chat message");

				break;
			} else if (ret == 0) {
				break;
			}

			ChatMessage *msg = unserialise_chat_message(&serialised);

			fprintf(stderr, "msg: %s\n", msg);

			freep(msg);
		}
	}

	if (pthread_join(client.heartbeat_thread, NULL) != 0) {
		log_fatal(ERROR_THREAD, "failed to join client heartbeat thread");
	}

	if (close(client.socket_fd) < 0) {
		if (errno == EBADF) {
			log_info("client disconnected");
		} else {
			log_errorf(ERROR_NETWORK, "failed to disconnect from client: %d", errno);
		}
	}

	return NULL;
}

int main() {
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_fd < 0) {
		log_fatal(ERROR_NETWORK, "failed to construct socket");
	}

	struct sockaddr_in server_addr = {
	    .sin_family = AF_INET, .sin_port = htons(5000), .sin_addr.s_addr = htonl(INADDR_ANY)};

	int opt_val = TRUE;

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val) < 0) {
		log_fatal(ERROR_NETWORK, "failed to set socket options");
	}

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0) {
		log_fatal(ERROR_NETWORK, "failed to bind to socket");
	}

	if (listen(server_fd, 128) < 0) {
		log_fatal(ERROR_NETWORK, "failed to listen on bound socket");
	}

	while (TRUE) {
		struct sockaddr_in client_addr = {0};
		socklen_t client_addr_len = sizeof client_addr;
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

		if (client_fd < 0) {
			log_fatal(ERROR_NETWORK, "failed to accept client connection");
		}

		pthread_t thread;

		if (pthread_create(&thread, NULL, client_handler, &client_fd) != 0) {
			log_fatal(ERROR_THREAD, "failed to start client handling thread");
		}

		if (pthread_detach(thread) != 0) {
			log_fatal(ERROR_THREAD, "failed to detach client handling thread");
		}
	}

	if (close(server_fd) < 0) {
		log_error(ERROR_NETWORK, "failed to shutdown server socket");
	}

	return EXIT_SUCCESS;
}
