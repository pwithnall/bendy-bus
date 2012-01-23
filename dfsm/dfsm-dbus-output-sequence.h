/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * D-Bus Simulator
 * Copyright (C) Philip Withnall 2012 <philip@tecnocode.co.uk>
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

#ifndef DFSM_DBUS_OUTPUT_SEQUENCE_H
#define DFSM_DBUS_OUTPUT_SEQUENCE_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <dfsm/dfsm-output-sequence.h>
#include <dfsm/dfsm-utils.h>

G_BEGIN_DECLS

#define DFSM_TYPE_DBUS_OUTPUT_SEQUENCE		(dfsm_dbus_output_sequence_get_type ())
#define DFSM_DBUS_OUTPUT_SEQUENCE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_DBUS_OUTPUT_SEQUENCE, DfsmDBusOutputSequence))
#define DFSM_DBUS_OUTPUT_SEQUENCE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_DBUS_OUTPUT_SEQUENCE, DfsmDBusOutputSequenceClass))
#define DFSM_IS_DBUS_OUTPUT_SEQUENCE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_DBUS_OUTPUT_SEQUENCE))
#define DFSM_IS_DBUS_OUTPUT_SEQUENCE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_DBUS_OUTPUT_SEQUENCE))
#define DFSM_DBUS_OUTPUT_SEQUENCE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_DBUS_OUTPUT_SEQUENCE, DfsmDBusOutputSequenceClass))

typedef struct _DfsmDBusOutputSequencePrivate	DfsmDBusOutputSequencePrivate;

typedef struct {
	GObject parent;
	DfsmDBusOutputSequencePrivate *priv;
} DfsmDBusOutputSequence;

typedef struct {
	GObjectClass parent;
} DfsmDBusOutputSequenceClass;

GType dfsm_dbus_output_sequence_get_type (void) G_GNUC_CONST;

DfsmDBusOutputSequence *dfsm_dbus_output_sequence_new (GDBusConnection *connection, const gchar *object_path,
                                                       GDBusMethodInvocation *invocation) DFSM_CONSTRUCTOR;

G_END_DECLS

#endif /* !DFSM_DBUS_OUTPUT_SEQUENCE_H */
