#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "packets.h"

#define INPUT_NULL 0
#define INPUT_ESCAPE 27
#define INPUT_RIGHT 4414235
#define INPUT_LEFT 4479771
#define INPUT_END 5
#define INPUT_HOME 1
#define INPUT_LINE_FEED 10
#define INPUT_TAB 9
#define INPUT_ALT_LEFT 25115
#define INPUT_ALT_RIGHT 26139
#define INPUT_BACKSPACE 127
#define INPUT_DELETE 4

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
