/* See LICENSE file for copyright and license details. */
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <X11/keysym.h>
#include "main.h"
#include "keys.h"
#include "workspace.h"
#include "client.h"
#include "list.h"
#include "log.h"
#include "config.h"

#define OPT(_opt) (strcmp(_opt, key) == 0)
#define PATH_MAX 256
#define LINE_MAX 256

static void setopt(const char *key, char *val);
static void set_key(const char *key, char *val);

enum cfg_section { OPTION = 0, KEYBIND };

static const struct {
    enum cfg_section section;
    const char *key;
    void (*func)(const char *, char *);
} config[] = {
    {OPTION, "border_width", setopt},
    {OPTION, "move_step", setopt},
    {OPTION, "resize_step", setopt},
    {OPTION, "java_workaround", setopt},
    {OPTION, "cursor_position", setopt},
    {OPTION, "focus_color", setopt},
    {OPTION, "unfocus_color", setopt},
    {OPTION, "center_new_windows", setopt},
    {KEYBIND, "move_up", set_key},
    {KEYBIND, "move_down", set_key},
    {KEYBIND, "move_left", set_key},
    {KEYBIND, "move_right", set_key},
    {KEYBIND, "terminal1", set_key},
    {KEYBIND, "terminal2", set_key},
    {KEYBIND, "browser", set_key},
    {KEYBIND, "browser2", set_key},
    {KEYBIND, "launcher", set_key},
    {KEYBIND, "mpc_toggle", set_key},
    {KEYBIND, "mpc_seek_f", set_key},
    {KEYBIND, "mpc_seek_b", set_key},
    {KEYBIND, "mpc_next", set_key},
    {KEYBIND, "mpc_prev", set_key},
    {KEYBIND, "vol_up", set_key},
    {KEYBIND, "vol_down", set_key},
    {KEYBIND, "vol_up2", set_key},
    {KEYBIND, "vol_down2", set_key},
    {KEYBIND, "vol_toggle", set_key},
    {KEYBIND, "resize_grow_height", set_key},
    {KEYBIND, "resize_grow_width", set_key},
    {KEYBIND, "resize_shrink_height", set_key},
    {KEYBIND, "resize_shrink_width", set_key},
    {KEYBIND, "resize_grow_both", set_key},
    {KEYBIND, "resize_shrink_both", set_key},
    {KEYBIND, "cycle_prev", set_key},
    {KEYBIND, "cycle_next", set_key},
    {KEYBIND, "teleport_center", set_key},
    {KEYBIND, "teleport_top_left", set_key},
    {KEYBIND, "teleport_top_right", set_key},
    {KEYBIND, "teleport_bottom_left", set_key},
    {KEYBIND, "teleport_bottom_right", set_key},
    {KEYBIND, "fullscreen", set_key},
    {KEYBIND, "maximize_vert", set_key},
    {KEYBIND, "maximize_horz", set_key},
    {KEYBIND, "kill_win", set_key},
    {KEYBIND, "prior_workspace", set_key},
    {KEYBIND, "prev_workspace", set_key},
    {KEYBIND, "next_workspace", set_key},
    {KEYBIND, "restart", set_key},
    {KEYBIND, "quit", set_key},
    {KEYBIND, "select_workspace_1", set_key},
    {KEYBIND, "select_workspace_2", set_key},
    {KEYBIND, "select_workspace_3", set_key},
    {KEYBIND, "select_workspace_4", set_key},
    {KEYBIND, "select_workspace_5", set_key},
    {KEYBIND, "select_workspace_6", set_key},
    {KEYBIND, "select_workspace_7", set_key},
    {KEYBIND, "select_workspace_8", set_key},
    {KEYBIND, "select_workspace_9", set_key},
    {KEYBIND, "select_workspace_0", set_key},
    {KEYBIND, "send_to_workspace_1", set_key},
    {KEYBIND, "send_to_workspace_2", set_key},
    {KEYBIND, "send_to_workspace_3", set_key},
    {KEYBIND, "send_to_workspace_4", set_key},
    {KEYBIND, "send_to_workspace_5", set_key},
    {KEYBIND, "send_to_workspace_6", set_key},
    {KEYBIND, "send_to_workspace_7", set_key},
    {KEYBIND, "send_to_workspace_8", set_key},
    {KEYBIND, "send_to_workspace_9", set_key},
    {KEYBIND, "send_to_workspace_0", set_key},
    {KEYBIND, "move_up", set_key},
    {KEYBIND, "move_down", set_key},
    {KEYBIND, "move_left", set_key},
    {KEYBIND, "move_right", set_key},
    {KEYBIND, "maximize_half_left", set_key},
    {KEYBIND, "maximize_half_right", set_key},
    {KEYBIND, "maximize_half_bottom", set_key},
    {KEYBIND, "maximize_half_top", set_key},
};

static char terminal[] = "urxvt";
static char terminal2[] = "termite";
static char browser[] = "chromium";
static char browser2[] = "firefox";
static char launcher[] = "rofi -show run";
static char mpctoggle[] = "mpc -q toggle";
static char mpcseekf[] = "mpc -q seek +30";
static char mpcseekb[] = "mpc -q seek -30";
static char mpcnext[] = "mpc -q next";
static char mpcprev[] = "mpc -q prev";
static char volup[] = "amixer -q set Master 3%+ unmute";
static char voldown[] = "amixer -q set Master 3%- unmute";
static char voltoggle[] = "amixer -q set Master toggle";

int border_width = 2;
int move_step = 30;
int resize_step = 30;
int cursor_position = 0;
bool java_workaround = false;
bool center_new_windows = false;
char *focus_color;
char *unfocus_color;

static bool is_regular_file(char *path) {
    struct stat statbuf;

    if (stat(path, &statbuf) == 0)
        if (S_ISREG(statbuf.st_mode))
            return true;
    return false;
}

static char *skip_leading_space(char *s) {
    while (*s && isspace(*s))
        s++;
    return s;
}

static void set_key(const char *key, char *val) {
    uint16_t mod = 0;
    uint16_t keysym = 0;

    /* PRINTF("set_key: %s: %s\n", key, val); */

    /* parse modifier */
    char *token = strtok(val, "+");
    do {
        if (strlen(token) == 4 && strncasecmp("mod", token, 3) == 0) {
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
            /* else */
            /* 	keysym = XStringToKeysym(token); */
            if (keysym == 0) {
                warn("set_key: keysym for '%s' not found\n", token);
                return;
            }
        }
    } while ((token = strtok(NULL, "+")));

    if (OPT("terminal1")) {
        keys[0] = (Key){mod, keysym, spawn, {.com = terminal}};
    } else if (OPT("terminal2")) {
        keys[1] = (Key){mod, keysym, spawn, {.com = terminal2}};
    } else if (OPT("browser")) {
        keys[2] = (Key){mod, keysym, spawn, {.com = browser}};
    } else if (OPT("browser2")) {
        keys[3] = (Key){mod, keysym, spawn, {.com = browser2}};
    } else if (OPT("launcher")) {
        keys[4] = (Key){mod, keysym, spawn, {.com = launcher}};
    } else if (OPT("mpc_toggle")) {
        keys[5] = (Key){mod, keysym, spawn, {.com = mpctoggle}};
    } else if (OPT("mpc_seek_f")) {
        keys[6] = (Key){mod, keysym, spawn, {.com = mpcseekf}};
    } else if (OPT("mpc_seek_b")) {
        keys[7] = (Key){mod, keysym, spawn, {.com = mpcseekb}};
    } else if (OPT("mpc_next")) {
        keys[8] = (Key){mod, keysym, spawn, {.com = mpcnext}};
    } else if (OPT("mpc_prev")) {
        keys[9] = (Key){mod, keysym, spawn, {.com = mpcprev}};
    } else if (OPT("vol_up")) {
        keys[10] = (Key){mod, keysym, spawn, {.com = volup}};
    } else if (OPT("vol_down")) {
        keys[11] = (Key){mod, keysym, spawn, {.com = voldown}};
    } else if (OPT("vol_up2")) {
        keys[12] = (Key){mod, keysym, spawn, {.com = volup}};
    } else if (OPT("vol_down2")) {
        keys[13] = (Key){mod, keysym, spawn, {.com = voldown}};
    } else if (OPT("vol_toggle")) {
        keys[14] = (Key){mod, keysym, spawn, {.com = voltoggle}};
    } else if (OPT("resize_grow_height")) {
        keys[15] = (Key){mod, keysym, resize, {.i = GrowHeight}};
    } else if (OPT("resize_grow_width")) {
        keys[16] = (Key){mod, keysym, resize, {.i = GrowWidth}};
    } else if (OPT("resize_shrink_height")) {
        keys[17] = (Key){mod, keysym, resize, {.i = ShrinkHeight}};
    } else if (OPT("resize_shrink_width")) {
        keys[18] = (Key){mod, keysym, resize, {.i = ShrinkWidth}};
    } else if (OPT("resize_grow_both")) {
        keys[19] = (Key){mod, keysym, resize, {.i = GrowBoth}};
    } else if (OPT("resize_shrink_both")) {
        keys[20] = (Key){mod, keysym, resize, {.i = ShrinkBoth}};
    } else if (OPT("cycle_prev")) {
        keys[21] = (Key){mod, keysym, cycleclients, {.i = PrevWindow}};
    } else if (OPT("cycle_next")) {
        keys[22] = (Key){mod, keysym, cycleclients, {.i = NextWindow}};
    } else if (OPT("teleport_center")) {
        keys[23] = (Key){mod, keysym, teleport, {.i = Center}};
    } else if (OPT("teleport_top_left")) {
        keys[24] = (Key){mod, keysym, teleport, {.i = TopLeft}};
    } else if (OPT("teleport_top_right")) {
        keys[25] = (Key){mod, keysym, teleport, {.i = TopRight}};
    } else if (OPT("teleport_bottom_left")) {
        keys[26] = (Key){mod, keysym, teleport, {.i = BottomLeft}};
    } else if (OPT("teleport_bottom_right")) {
        keys[27] = (Key){mod, keysym, teleport, {.i = BottomRight}};
    } else if (OPT("fullscreen")) {
        keys[28] = (Key){mod, keysym, maximize, {.i = 0}};
    } else if (OPT("maximize_vert")) {
        keys[29] = (Key){mod, keysym, maximizeaxis, {.i = MaxVertical}};
    } else if (OPT("maximize_horz")) {
        keys[30] = (Key){mod, keysym, maximizeaxis, {.i = MaxHorizontal}};
    } else if (OPT("kill_win")) {
        keys[31] = (Key){mod, keysym, killselected, {.i = 0}};
    } else if (OPT("prior_workspace")) {
        keys[32] = (Key){mod, keysym, selectrws, {.i = LastWorkspace}};
    } else if (OPT("prev_workspace")) {
        keys[33] = (Key){mod, keysym, selectrws, {.i = PrevWorkspace}};
    } else if (OPT("next_workspace")) {
        keys[34] = (Key){mod, keysym, selectrws, {.i = NextWorkspace}};
    } else if (OPT("restart")) {
        keys[35] = (Key){mod, keysym, restart, {.i = 0}};
    } else if (OPT("quit")) {
        keys[36] = (Key){mod, keysym, quit, {.i = 0}};
    } else if (OPT("move_up")) {
        keys[57] = (Key){mod, keysym, move, {.i = MoveUp}};
    } else if (OPT("move_down")) {
        keys[58] = (Key){mod, keysym, move, {.i = MoveDown}};
    } else if (OPT("move_left")) {
        keys[59] = (Key){mod, keysym, move, {.i = MoveLeft}};
    } else if (OPT("move_right")) {
        keys[60] = (Key){mod, keysym, move, {.i = MoveRight}};
    } else if (OPT("maximize_half_left")) {
        keys[61] = (Key){mod, keysym, maximize_half, {.i = Left}};
    } else if (OPT("maximize_half_right")) {
        keys[62] = (Key){mod, keysym, maximize_half, {.i = Right}};
    } else if (OPT("maximize_half_bottom")) {
        keys[63] = (Key){mod, keysym, maximize_half, {.i = Bottom}};
    } else if (OPT("maximize_half_top")) {
        keys[64] = (Key){mod, keysym, maximize_half, {.i = Top}};
    }
}

static void setopt(const char *key, char *val) {
    /* PRINTF("setopt: %s: %s\n", key, val); */

    if (OPT("border_width")) {
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
    } else if (OPT("java_workaround")) {
        java_workaround = (atoi(val) != 0);
    } else if (OPT("cursor_position")) {
        cursor_position = atoi(val);
        if (cursor_position < 0)
            cursor_position = 0;
    } else if (OPT("center_new_windows")) {
        center_new_windows = (atoi(val) != 0);
    } else {
        warn("setopt: no handler for %s\n", key);
    }
}

char *find_config(const char *file) {
    char *path = (char *)malloc(PATH_MAX);

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

/* return the line number of the first parsing error.
 * return -1 on file open error.
 * return 0 otherwise.  */
int parse_config(const char *fname) {
    FILE *file = NULL;
    int err = 0;
    char line[LINE_MAX];
    char *p = NULL;
    size_t toklen = 0;
    int cfg_idx = -1;
    int line_num = 0;
    enum cfg_section section = OPTION;

    if (!(file = fopen(fname, "r")))
        return -1;
    PRINTF("loading config: %s\n", fname);

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

        for (int i = 0; i < LENGTH(config); ++i) {
            if (strncasecmp(p, config[i].key, toklen) == 0 &&
                strlen(config[i].key) == toklen)
                cfg_idx = i;
        }

        if (cfg_idx == -1) {
            warn("%s: couldn't match setting key on line %d\n", fname,
                 line_num);
            if (err == 0)
                err = line_num;
            continue;
        }

        p += toklen;
        p += strspn(p, "= \t\n");

        char *val = p;
        if (strlen(val) == 0) {
            warn("%s: missing value for %s on line %d\n", fname,
                 config[cfg_idx].key, line_num);
            if (err == 0)
                err = line_num;
            continue;
        }

        /* trim trailing newline */
        if (val[strlen(val) - 1] == '\n') {
            val[strlen(val) - 1] = '\0';
        }

        if (section == OPTION)
            config[cfg_idx].func(config[cfg_idx].key, val);
        else if (section == KEYBIND)
            config[cfg_idx].func(config[cfg_idx].key, val);
    }

    fclose(file);
    return err;
}
