#include "server.h"

#include "packets.h"
#include "utils.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static void *heartbeat(void *arg);
static void *client_handler(void *arg);

static void *heartbeat(void *arg) {
	Client *client = (Client *)arg;

	while (TRUE) {
		client->heartbeat_status = HeartbeatStatusPing;

		if (send(client->socket_fd, &client->heartbeat_status, sizeof client->heartbeat_status, 0) < 0) {
			break;
		}

		// Wait for client_handler to receive pong.
		sleep(HEARTBEAT_INTERVAL);

		if (client->heartbeat_status != HeartbeatStatusPong) {
			log_error(ERROR_HEARTBEAT, "client has not responsed to last ping with a pong");

			shutdown(client->socket_fd, SHUT_RDWR);

			break;
		}
	}

	return NULL;
}

static void *client_handler(void *arg) {
	Client client = {.socket_fd = *(int *)arg};

	printf("Connected to client\n");

	if (pthread_create(&client.heartbeat_thread, NULL, heartbeat, &client) != 0) {
		log_fatal(ERROR_THREAD, "failed to create client heartbeat thread");
	}

	if (pthread_detach(client.heartbeat_thread) != 0) {
		log_fatal(ERROR_THREAD, "failed to detach client heartbeat thread");
	}

	while (TRUE) {
		PacketType packet_type;
		int n = recv(client.socket_fd, &packet_type, sizeof packet_type, 0);

		if (n < 1) {
			break;
		}

		if (packet_type == PacketTypeHeartbeat) {
			printf("received pong packet\n");

			Heartbeat heartbeat;
			int n = recv(client.socket_fd, &heartbeat, sizeof heartbeat, 0);

			if (n < 1) {
				break;
			}

			client.heartbeat_status = heartbeat.status;
		} else if (packet_type == PacketTypeChatMessage) {
			printf("received chat message\n");

			ChatMessage msg = {0};
			int n = recv(client.socket_fd, &msg.size, sizeof msg.size, 0);

			if (n < 1) {
				break;
			}

			msg.msg = calloc(1, msg.size);
			n = recv(client.socket_fd, msg.msg, msg.size, 0);

			if (n < 1) {
				break;
			}

			fprintf(stderr, "msg: %s\n", msg.msg);

			freep(&msg.msg);
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

	return EXIT_SUCCESS;
}
