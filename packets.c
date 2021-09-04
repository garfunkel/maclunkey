#include <errno.h>
#include <packets.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h> // TODO REMOVE
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <utils.h>

int send_packet(const int socket_fd, const Serialised *serialised, pthread_mutex_t *mutex) {
	int total_bytes = 0;
	int num_bytes = 0;
	int ret = 0;

	if (mutex != NULL) {
		pthread_mutex_lock(mutex);
	}

	while (total_bytes < serialised->size) {
		if ((num_bytes = send(socket_fd, (char *)serialised->data + total_bytes, serialised->size - total_bytes, 0)) <
		    0) {
			log_errorf(ERROR_NETWORK, "failed to send network packet of size %d", serialised->size);

			ret = num_bytes;

			break;
		} else if (num_bytes == 0) {
			ret = 0;

			break;
		} else {
			total_bytes += num_bytes;
			ret = total_bytes;
		}
	}

	if (mutex != NULL) {
		pthread_mutex_unlock(mutex);
	}

	return ret;
}

int recv_packet(const int socket_fd, Serialised *serialised, pthread_mutex_t *mutex) {
	char data[sizeof(PacketType) + sizeof serialised->size];
	int num_bytes = 0;

	if (mutex != NULL) {
		pthread_mutex_lock(mutex);
	}

	if ((num_bytes = recv(socket_fd, data, sizeof(PacketType) + sizeof serialised->size, MSG_PEEK)) < 0) {
		log_errorf(ERROR_NETWORK, "failed to receive network packet of size %d", serialised->size);
		perror("HERE");

		free(serialised);

		if (mutex != NULL) {
			pthread_mutex_unlock(mutex);
		}

		return num_bytes;
	} else if (num_bytes == 0) {
		free(serialised);

		if (mutex != NULL) {
			pthread_mutex_unlock(mutex);
		}

		return 0;
	}

	memcpy(&serialised->size, data + sizeof(PacketType), sizeof serialised->size);
	serialised->data = malloc(serialised->size);
	int total_bytes = 0;
	num_bytes = 0;

	while (total_bytes < serialised->size) {
		if ((num_bytes = recv(socket_fd, (char *)serialised->data + total_bytes, serialised->size - total_bytes, 0)) <
		    0) {
			free(serialised->data);
			free(serialised);

			log_errorf(ERROR_NETWORK, "failed to receive network packet of size %d", serialised->size);

			if (mutex != NULL) {
				pthread_mutex_unlock(mutex);
			}

			return num_bytes;
		} else if (num_bytes == 0) {
			free(serialised->data);
			free(serialised);

			if (mutex != NULL) {
				pthread_mutex_unlock(mutex);
			}

			return 0;
		} else {
			total_bytes += num_bytes;
		}
	}

	if (mutex != NULL) {
		pthread_mutex_unlock(mutex);
	}

	return total_bytes;
}

Serialised *serialise_config(const Config *config) {
	PacketType packet_type = PacketTypeConfig;
	Serialised *serialised = malloc(sizeof *serialised);
	serialised->size = sizeof packet_type + sizeof serialised->size;

	for (size_t i = 0; i < config->num_rooms; i++) {
		serialised->size += strlen(config->rooms[i].name) + 1;
		serialised->size += strlen(config->rooms[i].desc) + 1;
	}

	serialised->data = malloc(serialised->size);
	char *pos = mempcpy(serialised->data, &packet_type, sizeof packet_type);
	pos = mempcpy(pos, &serialised->size, sizeof serialised->size);

	for (size_t i = 0; i < config->num_rooms; i++) {
		pos = mempcpy(pos, config->rooms[i].name, strlen(config->rooms[i].name) + 1);
		pos = mempcpy(pos, config->rooms[i].desc, strlen(config->rooms[i].desc) + 1);
	}

	return serialised;
}

Serialised *serialise_heartbeat(const Heartbeat heartbeat) {
	PacketType packet_type = PacketTypeHeartbeat;
	Serialised *serialised = malloc(sizeof *serialised);
	serialised->size = sizeof packet_type + sizeof serialised->size + sizeof heartbeat;
	serialised->data = malloc(serialised->size);

	char *pos = mempcpy(serialised->data, &packet_type, sizeof packet_type);
	pos = mempcpy(pos, &serialised->size, sizeof serialised->size);
	pos = mempcpy(pos, &heartbeat, sizeof heartbeat);

	return serialised;
}

Serialised *serialise_chat_message(const ChatMessage *msg) {
	PacketType packet_type = PacketTypeChatMessage;
	Serialised *serialised = malloc(sizeof *serialised);
	serialised->size = sizeof packet_type + sizeof serialised->size + strlen(msg) + 1;
	serialised->data = malloc(serialised->size);

	char *pos = mempcpy(serialised->data, &packet_type, sizeof packet_type);
	pos = mempcpy(pos, &serialised->size, sizeof serialised->size);
	pos = mempcpy(pos, msg, strlen(msg) + 1);

	return serialised;
}

Config *unserialise_config(const Serialised *serialised) {
	Config *config = calloc(1, sizeof *config);
	char *pos = (char *)serialised->data + sizeof(PacketType) + sizeof serialised->size;

	for (int i = 0; pos - (char *)serialised->data < serialised->size; i++) {
		Room *room = malloc(sizeof *room);

		room->name = strdup(pos);
		pos += strlen(room->name) + 1;
		room->desc = strdup(pos);
		pos += strlen(room->desc) + 1;

		config->num_rooms++;
		config->rooms = realloc(config->rooms, sizeof *room * config->num_rooms);
		config->rooms[config->num_rooms - 1] = *room;
	}

	return config;
}

Heartbeat unserialise_heartbeat(const Serialised *serialised) {
	return ((char *)serialised->data)[sizeof(PacketType) + sizeof serialised->size];
}

ChatMessage *unserialise_chat_message(const Serialised *serialised) {
	size_t offset = sizeof(PacketType) + sizeof serialised->size;
	ChatMessage *msg = malloc(serialised->size - offset);
	char *pos = (char *)serialised->data + offset;

	return memcpy(msg, pos, serialised->size - offset);
}
