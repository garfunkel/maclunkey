#ifndef _SERVER_H_
#define _SERVER_H_

#include "packets.h"

#include <pthread.h>

typedef struct {
	int socket_fd;
	HeartbeatStatus heartbeat_status;
	pthread_t thread;
	pthread_t heartbeat_thread;
} Client;

#endif
