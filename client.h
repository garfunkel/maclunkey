#pragma once

#include "packets.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Keyboard input codes.
 */
#define INPUT_NULL 0
#define INPUT_ESCAPE 27
#define INPUT_UP 4283163
#define INPUT_DOWN 4348699
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
#define ROOM_LIST_COLUMN_NAME "Name"
#define ROOM_LIST_COLUMN_PARTICIPANTS "Participants"
#define ROOM_LIST_COLUMN_DESC "Description"
#define ROOM_LIST_LEFT_MARGIN 8
#define ROOM_LIST_RIGHT_MARGIN 8
#define ROOM_LIST_COLUMN_NAME_WIDTH 25
#define ROOM_LIST_COLUMN_PARTICIPANTS_WIDTH 15
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
