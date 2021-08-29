#include "utils.h"

#include "packets.h"

#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void do_nothing() {
	// empty
}

char *get_home_dir() {
	char *home_dir = getenv(ENV_HOME);

	if (home_dir == NULL) {
		struct passwd *pw = getpwuid(getuid());

		if (pw != NULL) {
			home_dir = pw->pw_dir;
		}
	}

	return home_dir;
}

char *join_path(const char *path, ...) {
	char *joined = NULL;
	va_list paths;
	va_start(paths, path);

	while (path != NULL) {
		size_t start_pos = 0;
		size_t end_pos = strlen(path);

		for (; start_pos < strlen(path) && path[start_pos] == PATH_SEPARATOR; start_pos++) {
		}

		if (joined == NULL && start_pos > 0) {
			start_pos--;
		}

		for (; end_pos > 0 && path[end_pos - 1] == PATH_SEPARATOR; end_pos--) {
		}

		if (joined == NULL) {
			joined = strndup(path + start_pos, end_pos - start_pos + 1);
		} else {
			size_t joined_len = strlen(joined);

			joined = realloc(joined, joined_len + end_pos - start_pos + 2);
			joined[joined_len] = PATH_SEPARATOR;
			joined[joined_len + 1] = '\0';
			strncat(joined, path + start_pos, end_pos - start_pos);
		}

		path = va_arg(paths, char *);
	}

	va_end(paths);

	return joined;
}

char *strip_whitespace(const char *string) {
	int start_pos = 0;
	int end_pos = strlen(string);

	for (; start_pos < end_pos && isspace(string[start_pos]) != 0; start_pos++) {
	}

	for (; end_pos > 0 && isspace(string[end_pos - 1]) != 0; end_pos--) {
	}

	return strndup(string + start_pos, end_pos - start_pos);
}
