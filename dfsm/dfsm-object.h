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

#ifndef DFSM_OBJECT_H
#define DFSM_OBJECT_H

#include <glib.h>
#include <glib-object.h>

#include "dfsm-machine.h"

G_BEGIN_DECLS

#define DFSM_TYPE_OBJECT		(dfsm_object_get_type ())
#define DFSM_OBJECT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_OBJECT, DfsmObject))
#define DFSM_OBJECT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_OBJECT, DfsmObjectClass))
#define DFSM_IS_OBJECT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_OBJECT))
#define DFSM_IS_OBJECT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_OBJECT))
#define DFSM_OBJECT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_OBJECT, DfsmObjectClass))

typedef struct _DfsmObjectPrivate	DfsmObjectPrivate;

typedef struct {
	GObject parent;
	DfsmObjectPrivate *priv;
} DfsmObject;

typedef struct {
	GObjectClass parent;
} DfsmObjectClass;

GType dfsm_object_get_type (void) G_GNUC_CONST;

GPtrArray/*<DfsmObject>*/ *dfsm_object_factory_from_files (const gchar *simulation_code, const gchar *introspection_xml,
                                                           GError **error) DFSM_CONSTRUCTOR;

void dfsm_object_register_on_bus (DfsmObject *self, GDBusConnection *connection, GError **error);
void dfsm_object_unregister_on_bus (DfsmObject *self);

GDBusConnection *dfsm_object_get_connection (DfsmObject *self) G_GNUC_PURE;
DfsmMachine *dfsm_object_get_machine (DfsmObject *self) G_GNUC_PURE;
const gchar *dfsm_object_get_object_path (DfsmObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_object_get_well_known_bus_names (DfsmObject *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_OBJECT_H */
