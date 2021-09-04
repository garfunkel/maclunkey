#pragma once

#include "packets.h"

#include <pthread.h>

#define CONFIG_COMMENT '#'
#define CONFIG_SECTION_START '['
#define CONFIG_SECTION_END ']'

typedef struct {
	int socket_fd;
	Heartbeat heartbeat;
	pthread_t thread;
	pthread_t heartbeat_thread;
	pthread_mutex_t socket_lock;
} Client;
