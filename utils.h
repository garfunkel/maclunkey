#ifndef _UTILS_H_
#define _UTILS_H_

#define FALSE 0
#define TRUE 1
#define MAX_PARTICIPANTS 8
#define HEARTBEAT_INTERVAL 5

#define log_x(type, id, err) \
	fprintf(stderr, "%s: %s:%s:%d: %s: %s\n", type, __FILE__, __func__, __LINE__, error_to_string(id), err)

#define log_fatal(id, err)   \
	log_x("FATAL", id, err); \
	exit(id)

#define log_error(id, err) log_x("ERROR", id, err)

#endif
