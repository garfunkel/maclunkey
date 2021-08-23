#include "packets.h"
#include "utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

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

	while (TRUE) {
		Heartbeat heartbeat;
		int n = recv(socket_fd, &heartbeat, sizeof(heartbeat), 0);

		if (n <= 0) {
			break;
		}

		printf("recv: %d\n", n);

		if (heartbeat.status == HeartbeatStatusPing) {
			PacketType packet_type = PacketTypeHeartbeat;
			heartbeat.status = HeartbeatStatusPong;

			printf("ponging\n");

			send(socket_fd, &packet_type, sizeof(packet_type), 0);
			send(socket_fd, &heartbeat, sizeof(heartbeat), 0);
		}
	}

	return EXIT_SUCCESS;
}
