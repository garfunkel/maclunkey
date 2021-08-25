#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "packets.h"

#define CHAR_ESCAPE 27
#define CHAR_ESCAPE_FUNCTION 91
#define CHAR_ESCAPE_RIGHT 67
#define CHAR_ESCAPE_LEFT 68
#define CHAR_ESCAPE_END 70
#define CHAR_ESCAPE_HOME 72
#define CHAR_ESCAPE_ALT_LEFT 98
#define CHAR_ESCAPE_ALT_RIGHT 102

#define CHAR_HOME 1
#define CHAR_END 5

#define CHAT_PROMPT "Chat: "
#define CHAT_COL_START 7

typedef struct {
	unsigned int size;
	unsigned int cursor_pos;
	char *msg;
} ChatBuffer;

void send_chat_message(int socket_fd, ChatMessage *msg);
void setup_ui();

#endif
