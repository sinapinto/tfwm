/* See LICENSE file for copyright and license details. */
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *find_config(char *);
void parse_config(char *);

char *
find_config(char *file) {
	struct stat statbuf;
	char *full;
	char *prefix;

	full = (char *)malloc(PATH_MAX);

	if ((prefix = getenv("XDG_CONFIG_HOME"))) {
		snprintf(full, PATH_MAX, "%s/.%s", prefix, file);
		if (stat(full, &statbuf) == 0)
			if (S_ISREG(statbuf.st_mode))
				return full;
	}

	if ((prefix = getenv("HOME"))) {
		snprintf(full, PATH_MAX, "%s/.%s", prefix, file);
		if (stat(full, &statbuf) == 0)
			if (S_ISREG(statbuf.st_mode))
				return full;
	}

	if ((prefix = getpwuid(getuid())->pw_dir)) {
		snprintf(full, PATH_MAX, "%s/.%s", prefix, file);
		if (stat(full, &statbuf) == 0)
			if (S_ISREG(statbuf.st_mode))
				return full;
	}

	return NULL;
}

void
parse_config(char *fname) {
	FILE *f = NULL;
	char *line = NULL;
	size_t n = 0;
        ssize_t read;

	if (!fname)
		return;
	if (!(f = fopen(fname, "r")))
                return;

	while ((read = getline(&line, &n, f)) != 1) {
		printf("%s", line);
	}

        free(line);
	fclose(f);
}

int main(void) {
	char *rc_path = NULL;

	rc_path = find_config("tfwmrc");

	if (!rc_path)
		return 1;

	parse_config(rc_path);

	free(rc_path);
	return 0;
}

