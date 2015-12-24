/* See LICENSE file for copyright and license details. */
#include <sys/stat.h>
#include <string.h>
#include <strings.h> /* strncasecmp() */
#include <stdio.h>
#include <stdbool.h>
#include "tfwm.h"
#include "config.h"

#define PATH_MAX 4096
#define LINE_MAX 4096

static void setopt(char *key, char *val);
static bool is_regular_file(char *path);

bool pixmap_border   = false;
int border_width     = 4;
int move_step        = 30;
int resize_step      = 30;
bool sloppy_focus    = false;
bool java_workaround = true;
int cursor_position  = 0;
char *focus_color;
char *unfocus_color;

struct {
	char * key;
	void (*func)(char *, char *);
} config[] = {
	{ "pixmap_border",      setopt },
	{ "border_width",       setopt },
	{ "move_step",          setopt },
	{ "resize_step",        setopt },
	{ "sloppy_focus",       setopt },
	{ "java_workaround",    setopt },
	{ "cursor_position",    setopt },
	{ "focus_color",        setopt },
	{ "unfocus_color",      setopt },
};

bool
is_regular_file(char *path) {
	struct stat statbuf;

	if (stat(path, &statbuf) == 0)
		if (S_ISREG(statbuf.st_mode))
			return true;
	return false;
}

char *
find_config(char *file) {
	char *path;

	path = (char *)malloc(PATH_MAX);

	if (getenv("HOME")) {
		snprintf(path, PATH_MAX, "%s/.%s", getenv("HOME"), file);
		if (is_regular_file(path))
			return path;
	}

	if (getenv("XDG_CONFIG_HOME")) {
		snprintf(path, PATH_MAX, "%s/.%s", getenv("HOME"), file);
		if (is_regular_file(path))
			return path;
	}
	return NULL;
}

void setopt(char *key, char *val) {
	PRINTF("Config: %s: %s\n", key, val);

#define OPT(_opt) (strcmp(_opt, key) == 0)
	if (OPT("pixmap_border")) {
		pixmap_border = (atoi(val) != 0);
	} else if (OPT("border_width")) {
		border_width = atoi(val);
		if (border_width < 0)
			border_width = 0;
	} else if (OPT("focus_color")) {
                focus_color = malloc(strlen(val) + 1);
		snprintf(focus_color, strlen(val) + 1, "%s", val);
	} else if (OPT("unfocus_color")) {
                unfocus_color = malloc(strlen(val) + 1);
		snprintf(unfocus_color, strlen(val) + 1, "%s", val);
	} else if (OPT("move_step")) {
		move_step = atoi(val);
		if (move_step < 0)
			move_step = 0;
	} else if (OPT("resize_step")) {
		resize_step = atoi(val);
		if (resize_step < 0)
			resize_step = 0;
	} else if (OPT("sloppy_focus")) {
		sloppy_focus = (atoi(val) != 0);
	} else if (OPT("java_workaround")) {
		java_workaround = (atoi(val) != 0);
	} else if (OPT("cursor_position")) {
		cursor_position = atoi(val);
		if (cursor_position < 0)
			cursor_position = 0;
	} else {
		warn("setopt: no handler for %s\n", key);
	}
}

int
parse_config(char *fname) {
	FILE *file = NULL;
	char line[LINE_MAX];
	char *p = NULL;
	size_t token_len = 0;
	int cfg_idx = -1;
	char *val = NULL;
	unsigned int i;
	int line_num = 0;

	if (!fname)
		return 1;
	if (!(file = fopen(fname, "r")))
		return 1;

	while (fgets(line, LINE_MAX, file)) {
		line_num++;
		p = line;

		while (*p && (*p == ' ' || *p == '\t'
			      || *p == '\n' || *p == '\r'))
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
			warn("%s: couldn't match setting key on line %d\n",
			     fname, line_num);
			return 1;
		}

		p += token_len;
		p += strspn(p, "= \t\n");

		val = p;
		if (strlen(val) == 0) {
			warn("%s: missing value for %s on line %d\n",
			     fname, config[cfg_idx].key, line_num);
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

