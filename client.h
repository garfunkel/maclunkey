#pragma once

#include "packets.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Error handling gubbins.
 */
#define ERR_TABLE(ERR)                    \
	ERR(ERROR_NETWORK, "Network error")   \
	ERR(ERROR_TERMINAL, "Terminal error") \
	ERR(ERROR_THREAD, "Thread error")     \
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

/*
 * Keyboard input codes.
 */
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
#define INPUT_CTRL_D 4
#define INPUT_DELETE 2117294875

/*
 * Global constants.
 */
#define MIN_WINDOW_WIDTH 40
#define MIN_WINDOW_HEIGHT 18
#define CHAT_BOX_WIDTH 20
#define CHAT_COL_START strlen(CHAT_PROMPT) + 2

const char *PARTICIPANTS_TITLE = "Participants";
const char *CHAT_PROMPT = "Chat:";
const char *CHAT_TITLE = "Chat";

/*
 * Shortcuts for ANSI commands.
 */
const char *ANSI_CMD_ENABLE_ALTERNATE_BUFFER = "\033[?1049h";
const char *ANSI_CMD_DISABLE_ALTERNATE_BUFFER = "\033[?1049l";
const char *ANSI_CMD_CLEAR_SCREEN = "\033[2J";
const char *ANSI_CMD_CLEAR_LINE = "\033[0K";
const char *ANSI_CMD_CURSOR_RESET = "\033[H";
const char *ANSI_CMD_CURSOR_LEFT = "\033[1D";
const char *ANSI_CMD_CURSOR_RIGHT = "\033[1C";
