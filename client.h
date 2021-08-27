#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "packets.h"

#define INPUT_NULL 0
#define INPUT_ESCAPE 27
#define INPUT_LEFT 4479771
#define INPUT_RIGHT 4414235
#define INPUT_HOME 1
#define INPUT_HOME_2 4741915
#define INPUT_END 5
#define INPUT_END_2 4610843
#define INPUT_LINE_FEED 10
#define INPUT_TAB 9
#define INPUT_ALT_LEFT 25115
#define INPUT_ALT_RIGHT 26139
#define INPUT_BACKSPACE 127
#define INPUT_DELETE 4

#define MIN_WINDOW_WIDTH 40
#define MIN_WINDOW_HEIGHT 20
#define CHAT_PROMPT "Chat:"
#define PARTICIPANTS_TITLE "Participants"
#define CHAT_TITLE "Chat"
#define CHAT_BOX_WIDTH 20
#define CHAT_COL_START strlen(CHAT_PROMPT) + 2

const char *ANSI_CMD_ENABLE_ALTERNATE_BUFFER = "\033[?1049h";
const char *ANSI_CMD_DISABLE_ALTERNATE_BUFFER = "\033[?1049l";
const char *ANSI_CMD_CLEAR_SCREEN = "\033[2J";
const char *ANSI_CMD_CLEAR_LINE = "\033[0K";
const char *ANSI_CMD_CURSOR_LEFT = "\033[1D";
const char *ANSI_CMD_CURSOR_RIGHT = "\033[1C";

#endif
