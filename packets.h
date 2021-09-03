#pragma once

#include <stdint.h>
#include <sys/types.h>

typedef enum
{
	PacketTypeConfig,
	PacketTypeRoomAction,
	PacketTypeHeartbeat,
	PacketTypeChatMessage,
	PacketTypeAudioFrame,
	PacketTypeVideoFrame
} _PacketType;

typedef uint8_t PacketType;

typedef enum
{
	HeartbeatPing,
	HeartbeatPong
} _Heartbeat;

typedef uint8_t Heartbeat;

typedef struct {
	char *name;
	char *desc;
} Room;

typedef struct {
	uint16_t num_rooms;
	Room *rooms;
} Config;

typedef enum
{
	ActionJoinRoom,
	ActionLeaveRoom
} _Action;

typedef uint8_t Action;
typedef char ChatMessage;

typedef struct {
	uint16_t size;
	void *data;
} Serialised;

int send_packet(const int socket_fd, const Serialised *serialised, pthread_mutex_t *mutex);
int recv_packet(const int socket_fd, Serialised *serialised, pthread_mutex_t *mutex);

Serialised *serialise_config(const Config *config);
Serialised *serialise_action(const Action action);
Serialised *serialise_heartbeat(const Heartbeat heartbeat);
Serialised *serialise_chat_message(const ChatMessage *msg);

Config *unserialise_config(const Serialised *serialised);
Action unserialise_action(const Serialised *serialised);
Heartbeat unserialise_heartbeat(const Serialised *serialised);
ChatMessage *unserialise_chat_message(const Serialised *serialised);
