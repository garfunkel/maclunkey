#include "server.h"

#include "packets.h"
#include "utils.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

void *heartbeat(void *arg) {
	Client *client = (Client *)arg;

	while (TRUE) {
		printf("pinging\n");

		client->heartbeat_status = HeartbeatStatusPing;
		send(client->socket_fd, &client->heartbeat_status, sizeof(client->heartbeat_status), 0);

		sleep(5);

		if (client->heartbeat_status != HeartbeatStatusPong) {
			printf("client did not send back pong\n");

			shutdown(client->socket_fd, SHUT_RDWR);

			break;
		}
	}

	return NULL;
}

void *handle_client(void *arg) {
	Client *client = malloc(sizeof(Client));
	client->socket_fd = *(int *)arg;

	printf("Connected to client\n");

	pthread_create(&client->heartbeat_thread, NULL, heartbeat, client);

	while (TRUE) {
		PacketType packet_type;
		int n = recv(client->socket_fd, &packet_type, sizeof(packet_type), 0);

		if (n < 1) {
			break;
		}

		if (packet_type == PacketTypeHeartbeat) {
			printf("received pong packet\n");

			Heartbeat heartbeat;
			int n = recv(client->socket_fd, &heartbeat, sizeof(heartbeat), 0);

			if (n < 1) {
				break;
			}

			client->heartbeat_status = heartbeat.status;
		}
	}

	pthread_join(client->heartbeat_thread, NULL);

	free(client);

	return NULL;
}

int main() {
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server = {0};
	struct sockaddr_in client_addr = {0};

	if (server_fd < 0) {
		log_fatal("Could not create server socket.");
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(5000);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	int opt_val = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

	int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server));

	if (err < 0) {
		log_fatal("Could not bind().");
	}

	err = listen(server_fd, 128);

	if (err < 0) {
		log_fatal("Could not listen().");
	}

	while (TRUE) {
		socklen_t client_addr_len = sizeof(client_addr);

		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

		if (client_fd < 0) {
			log_fatal("Could not connect to client.");
		}

		pthread_t thread;
		pthread_create(&thread, NULL, handle_client, &client_fd);
		pthread_detach(thread);
	}
}
