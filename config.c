/* See LICENSE file for copyright and license details. */
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <X11/keysym.h>
#include "tfwm.h"
#include "config.h"
#include "workspace.h"
#include "client.h"
#include "list.h"
#include "util.h"

#define OPT(_opt) (strcmp(_opt, key) == 0)
#define PATH_MAX  256
#define LINE_MAX  256

static bool is_regular_file(char *path);
static void setopt(char *key, char *val);
static void setkey(char *key, char *val);

enum cfg_section {
	OPTION = 0,
	KEYBIND
};

static const struct {
	enum cfg_section section;
	char * key;
	void (*func)(char *, char *);
} config[] = {
	{ OPTION,  "pixmap_border",         setopt },
	{ OPTION,  "border_width",          setopt },
	{ OPTION,  "move_step",             setopt },
	{ OPTION,  "resize_step",           setopt },
	{ OPTION,  "sloppy_focus",          setopt },
	{ OPTION,  "java_workaround",       setopt },
	{ OPTION,  "cursor_position",       setopt },
	{ OPTION,  "focus_color",           setopt },
	{ OPTION,  "unfocus_color",         setopt },
	{ KEYBIND, "move_up",               setkey },
	{ KEYBIND, "move_down",             setkey },
	{ KEYBIND, "move_left",             setkey },
	{ KEYBIND, "move_right",            setkey },
	{ KEYBIND, "terminal1",             setkey },
	{ KEYBIND, "terminal2",             setkey },
	{ KEYBIND, "browser",               setkey },
	{ KEYBIND, "browser2",              setkey },
	{ KEYBIND, "launcher",              setkey },
	{ KEYBIND, "mpc_toggle",            setkey },
	{ KEYBIND, "mpc_seek_f",            setkey },
	{ KEYBIND, "mpc_seek_b",            setkey },
	{ KEYBIND, "mpc_next",              setkey },
	{ KEYBIND, "mpc_prev",              setkey },
	{ KEYBIND, "vol_up",                setkey },
	{ KEYBIND, "vol_down",              setkey },
	{ KEYBIND, "vol_up2",               setkey },
	{ KEYBIND, "vol_down2",             setkey },
	{ KEYBIND, "vol_toggle",            setkey },
	{ KEYBIND, "resize_grow_height",    setkey },
	{ KEYBIND, "resize_grow_width",     setkey },
	{ KEYBIND, "resize_shrink_height",  setkey },
	{ KEYBIND, "resize_shrink_width",   setkey },
	{ KEYBIND, "resize_grow_both",      setkey },
	{ KEYBIND, "resize_shrink_both",    setkey },
	{ KEYBIND, "cycle_prev",            setkey },
	{ KEYBIND, "cycle_next",            setkey },
	{ KEYBIND, "teleport_center",       setkey },
	{ KEYBIND, "teleport_top_left",     setkey },
	{ KEYBIND, "teleport_top_right",    setkey },
	{ KEYBIND, "teleport_bottom_left",  setkey },
	{ KEYBIND, "teleport_bottom_right", setkey },
	{ KEYBIND, "fullscreen",            setkey },
	{ KEYBIND, "maximize_vert",         setkey },
	{ KEYBIND, "maximize_horz",         setkey },
	{ KEYBIND, "kill_win",              setkey },
	{ KEYBIND, "prior_workspace",       setkey },
	{ KEYBIND, "prev_workspace",        setkey },
	{ KEYBIND, "next_workspace",        setkey },
	{ KEYBIND, "restart",               setkey },
	{ KEYBIND, "quit",                  setkey },
	{ KEYBIND, "select_workspace_1",    setkey },
	{ KEYBIND, "select_workspace_2",    setkey },
	{ KEYBIND, "select_workspace_3",    setkey },
	{ KEYBIND, "select_workspace_4",    setkey },
	{ KEYBIND, "select_workspace_5",    setkey },
	{ KEYBIND, "select_workspace_6",    setkey },
	{ KEYBIND, "select_workspace_7",    setkey },
	{ KEYBIND, "select_workspace_8",    setkey },
	{ KEYBIND, "select_workspace_9",    setkey },
	{ KEYBIND, "select_workspace_0",    setkey },
	{ KEYBIND, "send_to_workspace_1",   setkey },
	{ KEYBIND, "send_to_workspace_2",   setkey },
	{ KEYBIND, "send_to_workspace_3",   setkey },
	{ KEYBIND, "send_to_workspace_4",   setkey },
	{ KEYBIND, "send_to_workspace_5",   setkey },
	{ KEYBIND, "send_to_workspace_6",   setkey },
	{ KEYBIND, "send_to_workspace_7",   setkey },
	{ KEYBIND, "send_to_workspace_8",   setkey },
	{ KEYBIND, "send_to_workspace_9",   setkey },
	{ KEYBIND, "send_to_workspace_0",   setkey },
	{ KEYBIND, "move_up",               setkey },
	{ KEYBIND, "move_down",             setkey },
	{ KEYBIND, "move_left",             setkey },
	{ KEYBIND, "move_right",            setkey },
};

static char *terminal[]  = { "urxvt", NULL };
static char *terminal2[] = { "termite", NULL };
static char *browser[]   = { "chromium", NULL };
static char *browser2[]  = { "firefox", NULL };
static char *launcher[]  = { "rofi", "-show", "run",  NULL };
static char *mpctoggle[] = { "mpc", "-q", "toggle", NULL };
static char *mpcseekf[]  = { "mpc", "-q", "seek", "+30", NULL };
static char *mpcseekb[]  = { "mpc", "-q", "seek", "-30", NULL };
static char *mpcnext[]   = { "mpc", "-q", "next", NULL };
static char *mpcprev[]   = { "mpc", "-q", "prev", NULL };
static char *volup[]     = { "amixer", "-q", "set", "Master", "3%+", "unmute", NULL };
static char *voldown[]   = { "amixer", "-q", "set", "Master", "3%-", "unmute", NULL };
static char *voltoggle[] = { "amixer", "-q", "set", "Master", "toggle", NULL };

bool pixmap_border   = false;
int border_width     = 4;
int move_step        = 30;
int resize_step      = 30;
bool sloppy_focus    = false;
bool java_workaround = true;
int cursor_position  = 0;
char *focus_color;
char *unfocus_color;

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
		snprintf(path, PATH_MAX, "%s/.%s", getenv("XDG_CONFIG_HOME"), file);
		if (is_regular_file(path))
			return path;
	}
	return NULL;
}

void setkey(char *key, char *val) {
	char     *token;
	uint16_t  mod = 0;
	uint16_t  keysym = 0;

	/* PRINTF("setkey: %s: %s\n", key, val); */

	/* parse modifier */
	token = strtok(val, "+");
	do {
		if (strlen(token) == 4 &&
		    strncasecmp("mod", token, 3) == 0) {
			if (token[3] == '1')
				mod |= XCB_MOD_MASK_1;
			else if (token[3] == '2')
				mod |= XCB_MOD_MASK_2;
			else if (token[3] == '3')
				mod |= XCB_MOD_MASK_3;
			else if (token[3] == '4')
				mod |= XCB_MOD_MASK_4;
			else if (token[3] == '5')
				mod |= XCB_MOD_MASK_5;
		} else if (strncasecmp("ctrl", token, 4) == 0) {
			mod |= XCB_MOD_MASK_CONTROL;
		} else if (strncasecmp("shift", token, 5) == 0) {
			mod |= XCB_MOD_MASK_SHIFT;
		} else {
			if (strncmp(token, "0x", 2) == 0)
				keysym = atoi(token);
			else
				keysym = XStringToKeysym(token);
			if (keysym == 0) {
				warn("setkey: keysym for '%s' not found\n",
				     token);
				return;
			}
		}
	} while ((token = strtok(NULL, "+")));

	if (OPT("terminal1")) {
		keys[0]  = (Key){ mod, keysym, spawn, {.com=terminal} };
	} else if (OPT("terminal2")) {
		keys[1]  = (Key){ mod, keysym, spawn, {.com=terminal2}   };
	} else if (OPT("browser")) {
		keys[2]  = (Key){ mod, keysym, spawn, {.com=browser}     };
	} else if (OPT("browser2")) {
		keys[3]  = (Key){ mod, keysym, spawn, {.com=browser2}    };
	} else if (OPT("launcher")) {
		keys[4]  = (Key){ mod, keysym, spawn, {.com=launcher}    };
	} else if (OPT("mpc_toggle")) {
		keys[5]  = (Key){ mod, keysym, spawn, {.com=mpctoggle}   };
	} else if (OPT("mpc_seek_f")) {
		keys[6]  = (Key){ mod, keysym, spawn, {.com=mpcseekf}    };
	} else if (OPT("mpc_seek_b")) {
		keys[7]  = (Key){ mod, keysym, spawn, {.com=mpcseekb}    };
	} else if (OPT("mpc_next")) {
		keys[8]  = (Key){ mod, keysym, spawn, {.com=mpcnext}     };
	} else if (OPT("mpc_prev")) {
		keys[9]  = (Key){ mod, keysym, spawn, {.com=mpcprev}     };
	} else if (OPT("vol_up")) {
		keys[10] = (Key){ mod, keysym, spawn, {.com=volup}       };
	} else if (OPT("vol_down")) {
		keys[11] = (Key){ mod, keysym, spawn, {.com=voldown}     };
	} else if (OPT("vol_up2")) {
		keys[12] = (Key){ mod, keysym, spawn, {.com=volup}       };
	} else if (OPT("vol_down2")) {
		keys[13] = (Key){ mod, keysym, spawn, {.com=voldown}     };
	} else if (OPT("vol_toggle")) {
		keys[14] = (Key){ mod, keysym, spawn, {.com=voltoggle}   };
	} else if (OPT("resize_grow_height")) {
		keys[15] = (Key){ mod, keysym, resize, {.i=GrowHeight}    };
	} else if (OPT("resize_grow_width")) {
		keys[16] = (Key){ mod, keysym, resize, {.i=GrowWidth}     };
	} else if (OPT("resize_shrink_height")) {
		keys[17] = (Key){ mod, keysym, resize, {.i=ShrinkHeight}  };
	} else if (OPT("resize_shrink_width")) {
		keys[18] = (Key){ mod, keysym, resize, {.i=ShrinkWidth}   };
	} else if (OPT("resize_grow_both")) {
		keys[19] = (Key){ mod, keysym, resize, {.i=GrowBoth}      };
	} else if (OPT("resize_shrink_both")) {
		keys[20] = (Key){ mod, keysym, resize, {.i=ShrinkBoth}    };
	} else if (OPT("cycle_prev")) {
		keys[21] = (Key){ mod, keysym, cycleclients, {.i=PrevWindow} };
	} else if (OPT("cycle_next")) {
		keys[22] = (Key){ mod, keysym, cycleclients, {.i=NextWindow} };
	} else if (OPT("teleport_center")) {
		keys[23] = (Key){ mod, keysym, teleport, {.i=Center}        };
	} else if (OPT("teleport_top_left")) {
		keys[24] = (Key){ mod, keysym, teleport, {.i=TopLeft}       };
	} else if (OPT("teleport_top_right")) {
		keys[25] = (Key){ mod, keysym, teleport, {.i=TopRight}      };
	} else if (OPT("teleport_bottom_left")) {
		keys[26] = (Key){ mod, keysym, teleport, {.i=BottomLeft}    };
	} else if (OPT("teleport_bottom_right")) {
		keys[27] = (Key){ mod, keysym, teleport, {.i=BottomRight}   };
	} else if (OPT("fullscreen")) {
		keys[28] = (Key){ mod, keysym, maximize, {.i=0}             };
	} else if (OPT("maximize_vert")) {
		keys[29] = (Key){ mod, keysym, maximizeaxis, {.i=MaxVertical} };
	} else if (OPT("maximize_horz")) {
		keys[30] = (Key){ mod, keysym, maximizeaxis, {.i=MaxHorizontal} };
	} else if (OPT("kill_win")) {
		keys[31] = (Key){ mod, keysym, killselected, {.i=0} };
	} else if (OPT("prior_workspace")) {
		keys[32] = (Key){ mod, keysym, selectrws, {.i=LastWorkspace} };
	} else if (OPT("prev_workspace")) {
		keys[33] = (Key){ mod, keysym, selectrws, {.i=PrevWorkspace} };
	} else if (OPT("next_workspace")) {
		keys[34] = (Key){ mod, keysym, selectrws, {.i=NextWorkspace} };
	} else if (OPT("restart")) {
		keys[35] = (Key){ mod, keysym, restart, {.i=0} };
	} else if (OPT("quit")) {
		keys[36] = (Key){ mod, keysym, quit, {.i=0} };
	} else if (OPT("select_workspace_1")) {
		keys[37] = (Key){ mod, keysym, selectws, {.i=1} };
	} else if (OPT("select_workspace_2")) {
		keys[38] = (Key){ mod, keysym, selectws, {.i=2} };
	} else if (OPT("select_workspace_3")) {
		keys[39] = (Key){ mod, keysym, selectws, {.i=3} };
	} else if (OPT("select_workspace_4")) {
		keys[40] = (Key){ mod, keysym, selectws, {.i=4} };
	} else if (OPT("select_workspace_5")) {
		keys[41] = (Key){ mod, keysym, selectws, {.i=5} };
	} else if (OPT("select_workspace_6")) {
		keys[42] = (Key){ mod, keysym, selectws, {.i=6} };
	} else if (OPT("select_workspace_7")) {
		keys[43] = (Key){ mod, keysym, selectws, {.i=7} };
	} else if (OPT("select_workspace_8")) {
		keys[44] = (Key){ mod, keysym, selectws, {.i=8} };
	} else if (OPT("select_workspace_9")) {
		keys[45] = (Key){ mod, keysym, selectws, {.i=9} };
	} else if (OPT("select_workspace_0")) {
		keys[46] = (Key){ mod, keysym, selectws, {.i=0} };
	} else if (OPT("send_to_workspace_1")) {
		keys[47] = (Key){ mod, keysym, sendtows, {.i=1} };
	} else if (OPT("send_to_workspace_2")) {
		keys[48] = (Key){ mod, keysym, sendtows, {.i=2} };
	} else if (OPT("send_to_workspace_3")) {
		keys[49] = (Key){ mod, keysym, sendtows, {.i=3} };
	} else if (OPT("send_to_workspace_4")) {
		keys[50] = (Key){ mod, keysym, sendtows, {.i=4} };
	} else if (OPT("send_to_workspace_5")) {
		keys[51] = (Key){ mod, keysym, sendtows, {.i=5} };
	} else if (OPT("send_to_workspace_6")) {
		keys[52] = (Key){ mod, keysym, sendtows, {.i=6} };
	} else if (OPT("send_to_workspace_7")) {
		keys[53] = (Key){ mod, keysym, sendtows, {.i=7} };
	} else if (OPT("send_to_workspace_8")) {
		keys[54] = (Key){ mod, keysym, sendtows, {.i=8} };
	} else if (OPT("send_to_workspace_9")) {
		keys[55] = (Key){ mod, keysym, sendtows, {.i=9} };
	} else if (OPT("send_to_workspace_0")) {
		keys[56] = (Key){ mod, keysym, sendtows, {.i=0} };
	} else if (OPT("move_up")) {
		keys[57] = (Key){ mod, keysym, move, {.i=MoveUp} };
	} else if (OPT("move_down")) {
		keys[58] = (Key){ mod, keysym, move, {.i=MoveDown} };
	} else if (OPT("move_left")) {
		keys[59] = (Key){ mod, keysym, move, {.i=MoveLeft} };
	} else if (OPT("move_right")) {
		keys[60] = (Key){ mod, keysym, move, {.i=MoveRight} };
	}
}

void setopt(char *key, char *val) {
	PRINTF("setopt: %s: %s\n", key, val);

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

/* return the line number of the first parsing error.
 * return -1 on file open error.
 * return 0 otherwise.  */
int
parse_config(char *fname) {
	FILE             *file = NULL;
	int               err = 0;
	char              line[LINE_MAX];
	char              *p = NULL;
	size_t            toklen = 0;
	int               cfg_idx = -1;
	char             *val = NULL;
	unsigned int      i;
	int               line_num = 0;
	enum cfg_section  section = OPTION;

	if (!(file = fopen(fname, "r")))
		return -1;

	while (fgets(line, LINE_MAX, file)) {
		line_num++;

		p = skip_leading_space(line);

		/* comment or blank line */
		if (p[0] == '\0' || p[0] == '#' || p[0] == ';')
			continue;

		if (p[0] == '[') {
			p++;
			toklen = strcspn(p, "]\n\t ");
			if (strncasecmp(p, "options", toklen) == 0) {
				section = OPTION;
			} else if (strncasecmp(p, "keybinds", toklen) == 0) {
				section = KEYBIND;
			} else {
				err = line_num;
			}
			continue;
		}

		toklen = strcspn(p, "=\n\t ");
		if (toklen == 0)
			continue;

		for (i = 0; i < LENGTH(config); ++i) {
			if (//config[i].section == section &&
			    strncasecmp(p, config[i].key, toklen) == 0 &&
			    strlen(config[i].key) == toklen)
				cfg_idx = i;
		}
		
		if (cfg_idx == -1) {
			warn("%s: couldn't match setting key on line %d\n",
			     fname, line_num);
			if (err == 0)
				err = line_num;
			continue;
		}

		p += toklen;
		p += strspn(p, "= \t\n");

		val = p;
		if (strlen(val) == 0) {
			warn("%s: missing value for %s on line %d\n",
			     fname, config[cfg_idx].key, line_num);
			if (err == 0)
				err = line_num;
			continue;
		}

		/* trim trailing newline */
		if (val[strlen(val)-1] == '\n') {
			val[strlen(val)-1] = '\0';
		}

		if (section == OPTION)
			config[cfg_idx].func(config[cfg_idx].key, val);
		else if (section == KEYBIND)
			config[cfg_idx].func(config[cfg_idx].key, val);
	}

	fclose(file);
	return err;
}

