/* See LICENSE file for copyright and license details. */
#ifndef LAUNCH_H
#define LAUNCH_H

#include <libsn/sn-monitor.h>

void launch_application(const char *cmd, const bool notify);
void startup_event_cb(SnMonitorEvent *event, void *user_data);

#endif
