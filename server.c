#include "server.h"

#include "packets.h"
#include "utils.h"

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static void *heartbeat_handler(void *arg);
static void *client_handler(void *arg);

static void *heartbeat_handler(void *arg) {
	Client *client = (Client *)arg;
	struct sigaction disconnect_action = {.sa_handler = do_nothing};

	if (sigaction(SIGUSR1, &disconnect_action, NULL) < 0) {
		log_error(ERROR_TERMINAL, "failed to set SIGUSR1 client disconnection signal");
	}

	while (TRUE) {
		client->heartbeat_status = HeartbeatStatusPing;

		if (send(client->socket_fd, &client->heartbeat_status, sizeof client->heartbeat_status, 0) < 0) {
			break;
		}

		// Wait for client_handler to receive pong.
		sleep(HEARTBEAT_INTERVAL);

		if (client->heartbeat_status != HeartbeatStatusPong) {
			// Check if client has already disconnected.
			if (send(client->socket_fd, NULL, 0, 0) < 0) {
				if (errno == EBADF) {
					break;
				} else {
					log_error(ERROR_HEARTBEAT, "failed to check status of client");
				}
			}

			log_error(ERROR_HEARTBEAT, "client has not responsed to last ping with a pong");

			if (shutdown(client->socket_fd, SHUT_RDWR) < 0) {
				log_error(ERROR_NETWORK, "failed to disconnect from client");
			}

			break;
		}
	}

	return NULL;
}

static void *client_handler(void *arg) {
	Client client = {.socket_fd = *(int *)arg};

	log_info("connected to client");

	if (pthread_create(&client.heartbeat_thread, NULL, heartbeat_handler, &client) != 0) {
		log_fatal(ERROR_THREAD, "failed to create client heartbeat thread");
	}

	while (TRUE) {
		PacketType packet_type;
		int n = recv(client.socket_fd, &packet_type, sizeof packet_type, 0);

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
			Heartbeat heartbeat;
			int n = recv(client.socket_fd, &heartbeat, sizeof heartbeat, 0);

			if (n < 1) {
				log_error(ERROR_NETWORK, "failed to receive pong");

				break;
			}

			client.heartbeat_status = heartbeat.status;
		} else if (packet_type == PacketTypeChatMessage) {
			printf("received chat message\n");

			ChatMessage msg = {0};
			int n = recv(client.socket_fd, &msg.size, sizeof msg.size, 0);

			if (n < 1) {
				log_error(ERROR_NETWORK, "failed to receive chat message size");

				break;
			}

			msg.msg = calloc(1, msg.size);
			n = recv(client.socket_fd, msg.msg, msg.size, 0);

			if (n < 1) {
				log_error(ERROR_NETWORK, "failed to receive chat message");

				break;
			}

			fprintf(stderr, "msg: %s\n", msg.msg);

			freep(&msg.msg);
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

	struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(5000), .sin_addr.s_addr = htonl(INADDR_ANY)};

	int opt_val = TRUE;

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val) < 0) {
		log_fatal(ERROR_NETWORK, "failed to set socket options");
	}

	if (bind(server_fd, (struct sockaddr *)&server, sizeof server) < 0) {
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
