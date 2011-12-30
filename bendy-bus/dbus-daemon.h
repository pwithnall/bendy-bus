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
#include <glib-object.h>

#include "program-wrapper.h"

#ifndef DSIM_DBUS_DAEMON_H
#define DSIM_DBUS_DAEMON_H

G_BEGIN_DECLS

#define DSIM_TYPE_DBUS_DAEMON		(dsim_dbus_daemon_get_type ())
#define DSIM_DBUS_DAEMON(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DSIM_TYPE_DBUS_DAEMON, DsimDBusDaemon))
#define DSIM_DBUS_DAEMON_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DSIM_TYPE_DBUS_DAEMON, DsimDBusDaemonClass))
#define DSIM_IS_DBUS_DAEMON(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DSIM_TYPE_DBUS_DAEMON))
#define DSIM_IS_DBUS_DAEMON_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DSIM_TYPE_DBUS_DAEMON))
#define DSIM_DBUS_DAEMON_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DSIM_TYPE_DBUS_DAEMON, DsimDBusDaemonClass))

typedef struct _DsimDBusDaemonPrivate	DsimDBusDaemonPrivate;

typedef struct {
	DsimProgramWrapper parent;
	DsimDBusDaemonPrivate *priv;
} DsimDBusDaemon;

typedef struct {
	DsimProgramWrapperClass parent;
} DsimDBusDaemonClass;

GType dsim_dbus_daemon_get_type (void) G_GNUC_CONST;

DsimDBusDaemon *dsim_dbus_daemon_new (GFile *working_directory, GFile *configuration_file) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

const gchar *dsim_dbus_daemon_get_bus_address (DsimDBusDaemon *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DSIM_DBUS_DAEMON_H */
