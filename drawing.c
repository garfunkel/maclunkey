#include "drawing.h"

#include <locale.h>
#include <stdio.h>
#include <termios.h>
#include <utils.h>
#include <wchar.h>

void draw_init() {
	setlocale(LC_ALL, "");
}

void print_multi(const char *str, int n) {
	for (int i = 0; i < n; i++) {
		printf("%s", str);
	}
}

int draw_line(int x, int y, LineType type, int length, wchar_t ch) {
	int ret = -1;

	if (x >= 0 && y >= 0) {
		printf("\033[%d;%dH", y + 1, x + 1);
	} else if (x < 0) {
		printf("\033[%dH", y + 1);
	} else {
		printf("\033[%dG", x + 1);
	}

	if (type == LineTypeVertical) {
		for (int i = 0; i < length; i++) {
			printf("%lc\033[1B\033[1D", (wint_t)ch);
		}

		ret = 0;
	} else {
		for (int i = 0; i < length; i++) {
			printf("%lc", (wint_t)ch);
		}

		ret = 0;
	}

	return ret;
}
