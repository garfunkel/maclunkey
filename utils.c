#include "utils.h"

#include "packets.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

void log_fatal(char *error) {
	fprintf(stderr, "FATAL: %s\n", error);

	exit(EXIT_FAILURE);
}
