/* See LICENSE file for copyright and license details. */
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <pwd.h>
#include <string.h>
#include <strings.h> /* strncasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "tfwm.h"
#include "config.h"

char *find_config(char *);
int parse_config(char *);

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

        /* the home directory listed in the password db can be different
         * than $HOME i.e. if the user sets a custom $HOME */
	if ((prefix = getpwuid(getuid())->pw_dir)) {
		snprintf(full, PATH_MAX, "%s/.%s", prefix, file);
		if (stat(full, &statbuf) == 0)
			if (S_ISREG(statbuf.st_mode))
				return full;
	}

	return NULL;
}

void setopt(char *key, char *val) {
	/* printf("key: %s, val: %s\n", key, val); */
	if (strcmp(key, "double_border") == 0) {
		double_border = atoi(val);
		if (double_border < 0)
			double_border = 0;
	}
}

struct {
	char * key;
	void (*func)(char *key, char *val);
} config[] = {
	{ "double_border", setopt },
};


int
parse_config(char *fname) {
	FILE *file = NULL;
	char line[MAX_CANON];
	char *p = NULL;
	size_t token_len = 0;
	int cfg_idx = -1;
	char *val = NULL;
	unsigned int i;

	if (!fname)
		return 1;
	if (!(file = fopen(fname, "r")))
		return 1;

	while (fgets(line, MAX_CANON, file)) {
		p = line;

		while (*p && isspace(*p))
			p++;

		if (p[0] == '\0' || p[0] == '#')
			continue;

		token_len = strcspn(p, "=\n\t ");

		if (token_len == 0)
			continue;

		for (i = 0; i < LENGTH(config); ++i) {
			if (strncasecmp(p, config[i].key, token_len) == 0 &&
			    strlen(config[i].key) == token_len)
				cfg_idx = i;
		}
		
		if (cfg_idx == -1)
			return 1;

		p += token_len;
		p += strspn(p, "= \t\n");

		val = p;
		if (strlen(val) == 0) {
			/* no value */
			continue;
		}
		/* trim trailing newline */
		if (val[strlen(val)-1] == '\n') {
			val[strlen(val)-1] = '\0';
		}

		config[cfg_idx].func(config[cfg_idx].key, val);
	}

	fclose(file);
	return 0;
}

