#pragma once

#include <stdint.h>
#include <sys/types.h>

typedef enum
{
	PacketTypeConfig,
	PacketTypeStatusUpdate,
	PacketTypeHeartbeat,
	PacketTypeChatMessage,
	PacketTypeAudioFrame,
	PacketTypeVideoFrame
} _PacketType;

typedef uint8_t PacketType;

typedef enum
{
	HeartbeatStatusPing,
	HeartbeatStatusPong
} _HeartbeatStatus;

typedef uint8_t HeartbeatStatus;

typedef struct {
	char *name;
	char *desc;
} Room;

typedef struct {
	size_t num_rooms;
	Room *rooms;
} Config;

typedef HeartbeatStatus Heartbeat;
typedef char ChatMessage;

typedef struct {
	uint16_t size;
	void *data;
} Serialised;

int send_packet(const int socket_fd, const Serialised *serialised, pthread_mutex_t *mutex);
int recv_packet(const int socket_fd, Serialised *serialised, pthread_mutex_t *mutex);

Serialised *serialise_config(const Config *config);
Serialised *serialise_heartbeat(const Heartbeat *heartbeat);
Serialised *serialise_chat_message(const ChatMessage *msg);

Config *unserialise_config(const Serialised *serialised);
Heartbeat *unserialise_heartbeat(const Serialised *serialised);
ChatMessage *unserialise_chat_message(const Serialised *serialised);
