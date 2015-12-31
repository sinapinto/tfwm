/* See LICENSE file for copyright and license details. */
#include <sys/types.h>
#include <sys/wait.h>
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
run_program(const char *cmd) {
	if (fork() == 0) {
		setsid();
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
			err("execl failed: %s\n", cmd);
		}
		_exit(0);
	}
	wait(0);
}

char *
skip_leading_space(char *s) {
	while (*s && isspace(*s))
		s++;
	return s;
}

