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

#define OPT(opt) (strcmp(opt, key) == 0)

void setopt(char *key, char *val) {
	PRINTF("Config: %s: %s\n", key, val);

	if (OPT("double_border")) {
		double_border = (atoi(val) != 0);
	}
	else if (OPT("border_width")) {
		border_width = atoi(val);
		if (border_width < 0)
			border_width = 0;
	}
	else if (OPT("outer_border_width")) {
		outer_border_width = atoi(val);
		if (outer_border_width < 0)
			outer_border_width = 0;
	}
	else if (OPT("focus_color")) {
                focus_color = malloc(strlen(val) + 1);
		snprintf(focus_color, strlen(val) + 1, "%s", val);
	}
	else if (OPT("outer_color")) {
                outer_color = malloc(strlen(val) + 1);
		snprintf(outer_color, strlen(val) + 1, "%s", val);
	}
	else if (OPT("unfocus_color")) {
                unfocus_color = malloc(strlen(val) + 1);
		snprintf(unfocus_color, strlen(val) + 1, "%s", val);
	}
	else if (OPT("move_step")) {
		move_step = atoi(val);
		if (move_step < 0)
			move_step = 0;
	}
	else if (OPT("resize_step")) {
		resize_step = atoi(val);
		if (resize_step < 0)
			resize_step = 0;
	}
	else if (OPT("sloppy_focus")) {
		sloppy_focus = (atoi(val) != 0);
	}
	else if (OPT("java_workaround")) {
		java_workaround = (atoi(val) != 0);
	}
	else {
		warn("setopt: no handler for %s\n", key);
	}
}

struct {
	char * key;
	void (*func)(char *key, char *val);
} config[] = {
	{ "double_border",      setopt },
	{ "border_width",       setopt },
	{ "outer_border_width", setopt },
	{ "focus_color",        setopt },
	{ "outer_color",        setopt },
	{ "unfocus_color",      setopt },
	{ "move_step",          setopt },
	{ "resize_step",        setopt },
	{ "sloppy_focus",       setopt },
	{ "java_workaround",    setopt },
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
	int line_no = 0;

	if (!fname)
		return 1;
	if (!(file = fopen(fname, "r")))
		return 1;

	while (fgets(line, MAX_CANON, file)) {
		line_no++;
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
		
		if (cfg_idx == -1) {
			warn("%s: couldn't match setting on line %d\n", fname, line_no);
			return 1;
		}

		p += token_len;
		p += strspn(p, "= \t\n");

		val = p;
		if (strlen(val) == 0) {
			warn("%s: missing value for %s on line %d\n",
			     fname, config[cfg_idx].key, line_no);
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

