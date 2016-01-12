/* See LICENSE file for copyright and license details. */
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include "main.h"
#include "log.h"
#include "cursor.h"
#include "launch.h"

void launch_application(const char *cmd, const bool notification) {
    SnLauncherContext *context;

    if (notification) {
        context = sn_launcher_context_new(sndisplay, scrno);
        sn_launcher_context_set_name(context, __WM_NAME__);
        sn_launcher_context_set_description(context, "launch application");
        sn_launcher_context_initiate(context, __WM_NAME__, cmd, last_timestamp);

        PRINTF("launch_application: startup id: %s\n",
               sn_launcher_context_get_startup_id(context));
    }

    if (fork() == 0) {
        setsid();
        if (fork() == 0) {
            if (notification)
                sn_launcher_context_setup_child_process(context);
            execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
            err("execl failed: %s\n", cmd);
        }
        _exit(0);
    }

    wait(0);
    if (notification)
        cursor_set_window_cursor(screen->root, XC_WATCH);
}

void startup_event_cb(SnMonitorEvent *event, void *user_data) {
    (void)user_data;
    SnStartupSequence *sequence = sn_monitor_event_get_startup_sequence(event);

    switch (sn_monitor_event_get_type(event)) {
    case SN_MONITOR_EVENT_INITIATED:
    case SN_MONITOR_EVENT_CHANGED: {
        if (sn_monitor_event_get_type(event) == SN_MONITOR_EVENT_INITIATED) {
            PRINTF("Initiated sequence %s\n",
                   sn_startup_sequence_get_id(sequence));
        } else {
            PRINTF("Changed sequence %s\n",
                   sn_startup_sequence_get_id(sequence));
        }
        const char *s = sn_startup_sequence_get_id(sequence);
        PRINTF(" id %s\n", s ? s : "(unset)");
        PRINTF(" workspace %d\n", sn_startup_sequence_get_workspace(sequence));
        break;
    }
    case SN_MONITOR_EVENT_COMPLETED:
        PRINTF("Completed sequence %s\n", sn_startup_sequence_get_id(sequence));
        cursor_set_window_cursor(screen->root, XC_POINTER);
        break;
    case SN_MONITOR_EVENT_CANCELED:
        PRINTF("Canceled sequence %s\n", sn_startup_sequence_get_id(sequence));
        break;
    }
}
