#pragma once

#include <stddef.h>

#define CURSOR_CURRENT_POS -1

#define CHAR_HORIZONTAL_LINE 0x2501
#define CHAR_VERTICAL_LINE 0x2502

typedef enum
{
	LineTypeHorizontal,
	LineTypeVertical
} LineType;

void draw_init();
void print_multi(const char *str, int n);
int draw_line(int x, int y, LineType type, int length, wchar_t ch);
