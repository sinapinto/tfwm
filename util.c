/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "tfwm.h"
#include "util.h"

void
warn(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

__attribute__ ((noreturn)) void
err(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
spawn(const Arg *arg) {
	(void)arg;
	if (fork() == 0) {
		if (conn)
			close(screen->root);
		setsid();
		execvp((char*)arg->com[0], (char**)arg->com);
		err("execvp %s", ((char **)arg->com)[0]);
	}
}

char *
skip_leading_space(char* s) {
	while (*s && isspace(*s))
		s++;
	return s;
}

