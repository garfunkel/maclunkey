#pragma once

#include "packets.h"

#include <pthread.h>

/*
 * Error handling gubbins.
 */
#define ERR_TABLE(ERR)                       \
	ERR(ERROR_NETWORK, "Network error")      \
	ERR(ERROR_TERMINAL, "Terminal error")    \
	ERR(ERROR_THREAD, "Thread error")        \
	ERR(ERROR_HEARTBEAT, "Heartbeat error")  \
	ERR(ERROR_CONFIG, "Configuration error") \
	ERR(ERROR_OS, "OS-level error")          \
	ERR(ERROR_UNKNOWN, "Unknown error")

#define ERR_ID(id, string) id,
#define ERR_STRING(id, string) string,

enum ErrId
{ ERR_TABLE(ERR_ID) };

const char *error_to_string(const enum ErrId id) {
	static const char *table[] = {ERR_TABLE(ERR_STRING)};

	if (id < 0 || id >= ERROR_UNKNOWN) {
		return table[ERROR_UNKNOWN];
	}

	return table[id];
}

#undef ERR_ID
#undef ERR_STRING
#undef ERR_TABLE

#define CONFIG_COMMENT '#'
#define CONFIG_SECTION_START '['
#define CONFIG_SECTION_END ']'

typedef struct {
	int socket_fd;
	HeartbeatStatus heartbeat_status;
	pthread_t thread;
	pthread_t heartbeat_thread;
	pthread_mutex_t socket_lock;
} Client;
