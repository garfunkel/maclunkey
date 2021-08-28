#pragma once

#include <sys/types.h>

typedef enum
{
	PacketTypeStatusUpdate,
	PacketTypeHeartbeat,
	PacketTypeChatMessage,
	PacketTypeAudioFrame,
	PacketTypeVideoFrame
} PacketType;

typedef enum
{
	HeartbeatStatusPing,
	HeartbeatStatusPong
} HeartbeatStatus;

typedef struct {
	HeartbeatStatus status;
} Heartbeat;

typedef struct {
	size_t size;
	char *msg;
} ChatMessage;
