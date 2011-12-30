/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * D-Bus Simulator
 * Copyright (C) Philip Withnall 2011 <philip@tecnocode.co.uk>
 * 
 * D-Bus Simulator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * D-Bus Simulator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with D-Bus Simulator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>

#ifndef DSIM_LOGGING_H
#define DSIM_LOGGING_H

G_BEGIN_DECLS

typedef enum {
	DSIM_LOG_TEST_PROGRAM = 0,
	DSIM_LOG_DBUS_DAEMON = 1,
	DSIM_LOG_SIMULATOR = 2,
	DSIM_LOG_SIMULATOR_LIBRARY = 3,
} DsimLoggingDomain;

#define DSIM_NUM_LOGGING_DOMAINS 4

void dsim_logging_init (const gchar *test_program_log_file, gint test_program_log_fd,
                        const gchar *dbus_daemon_log_file, gint dbus_daemon_log_fd,
                        const gchar *simulator_log_file, gint simulator_log_fd, GError **error);
void dsim_logging_finalise (void);

const gchar *dsim_logging_get_domain_name (DsimLoggingDomain domain_id) G_GNUC_CONST;

G_END_DECLS

#endif /* !DSIM_LOGGING_H */
