#pragma once

#include <stddef.h>

#define APP_NAME "maclunkey"
#define FALSE 0
#define TRUE 1
#define MAX_PARTICIPANTS 8
#define HEARTBEAT_INTERVAL 5
#define PATH_SEPARATOR '/'
#define ENV_HOME "HOME"
#define CONFIG_PATH ".config/" APP_NAME "/" APP_NAME ".config"

#define LOG_LEVEL_INFO "INFO"
#define LOG_LEVEL_WARNING "WARNING"
#define LOG_LEVEL_ERROR "ERROR"
#define LOG_LEVEL_FATAL "FATAL"

#define ERR_TABLE(ERR)                       \
	ERR(ERROR_NETWORK, "Network error")      \
	ERR(ERROR_TERMINAL, "Terminal error")    \
	ERR(ERROR_THREAD, "Thread error")        \
	ERR(ERROR_HEARTBEAT, "Heartbeat error")  \
	ERR(ERROR_CONFIG, "Configuration error") \
	ERR(ERROR_OS, "OS-level error")          \
	ERR(ERROR_UNKNOWN, "Unknown error")

enum ErrId
{
#define ERR_ID(id, string) id,
	ERR_TABLE(ERR_ID)
#undef ERR_ID
};

const char *error_to_string(const enum ErrId id);

#define log_error_x(type, id, err) \
	fprintf(stderr, "%s: %s:%s:%d: %s: %s\n", type, __FILE__, __func__, __LINE__, error_to_string(id), err)

#define log_error_xf(type, id, fmt, ...)       \
	char *__log_string = NULL;                 \
	asprintf(&__log_string, fmt, __VA_ARGS__); \
	log_error_x(type, id, __log_string);       \
	free(__log_string);                        \
	__log_string = NULL

#define log_x(type, msg) fprintf(stderr, "%s: %s:%s:%d: %s\n", type, __FILE__, __func__, __LINE__, msg)

#define log_xf(type, fmt, ...)                 \
	char *__log_string = NULL;                 \
	asprintf(&__log_string, fmt, __VA_ARGS__); \
	log_x(type, fmt, __log_string);            \
	free(__log_string);                        \
	__log_string = NULL

#define log_fatal(id, err)                 \
	log_error_x(LOG_LEVEL_FATAL, id, err); \
	exit(id)

#define log_fatalf(id, fmt, ...)                         \
	log_error_xf(LOG_LEVEL_FATAL, id, fmt, __VA_ARGS__); \
	exit(id)

#define log_error(id, err) log_error_x(LOG_LEVEL_ERROR, id, err)
#define log_errorf(id, fmt, ...) log_error_xf(LOG_LEVEL_ERROR, id, fmt, __VA_ARGS__)

#ifdef DEBUG
	#define log_warning(msg) log_x(LOG_LEVEL_WARNING, msg)
	#define log_warningf(fmt, ...) log_xf(LOG_LEVEL_WARNING, fmt, __VA_ARGS__)
	#define log_info(msg) log_x(LOG_LEVEL_INFO, msg)
	#define log_infof(fmt, ...) log_xf(LOG_LEVEL_INFO, fmt, __VA_ARGS__)
#else
	#define log_warning(msg)
	#define log_warningf(fmt, ...)
	#define log_info(msg)
	#define log_infof(fmt, ...)
#endif

#define freep(ptr) \
	free(ptr);     \
                   \
	ptr = NULL

typedef enum
{
	ConfigSectionGlobal,
	ConfigSectionRooms
} ConfigSection;

/*
 * Placeholder function that does nothing on purpose.
 */
void do_nothing();

char *get_home_dir();
char *join_path(const char *path, ...);
char *strip_whitespace(const char *string);

#ifdef __APPLE__
	#define mempcpy(dest, src, n) (char *)memcpy(dest, src, n) + n;
#endif
