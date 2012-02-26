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

/**
 * DfsmSimulationStatus:
 * @DFSM_SIMULATION_STATUS_STOPPED: Simulation is not running.
 * @DFSM_SIMULATION_STATUS_STARTED: Simulation is running.
 *
 * The current status of the simulation. This is not equivalent to the current state number of the simulated DFSM.
 */
typedef enum {
	DFSM_SIMULATION_STATUS_STOPPED = 0,
	DFSM_SIMULATION_STATUS_STARTED,
} DfsmSimulationStatus;

#define DFSM_TYPE_SIMULATION_STATUS dfsm_simulation_status_get_type ()
GType dfsm_simulation_status_get_type (void) G_GNUC_CONST;

#define DFSM_TYPE_OBJECT		(dfsm_object_get_type ())
#define DFSM_OBJECT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_OBJECT, DfsmObject))
#define DFSM_OBJECT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_OBJECT, DfsmObjectClass))
#define DFSM_IS_OBJECT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_OBJECT))
#define DFSM_IS_OBJECT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_OBJECT))
#define DFSM_OBJECT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_OBJECT, DfsmObjectClass))

typedef struct _DfsmObjectPrivate	DfsmObjectPrivate;

/**
 * DfsmObject:
 *
 * All the fields in the #DfsmObject structure are private and should never be accessed directly.
 */
typedef struct {
	GObject parent;
	DfsmObjectPrivate *priv;
} DfsmObject;

/**
 * DfsmObjectClass:
 * @dbus_method_call: default handler for the #DfsmObject::dbus-method-call signal
 * @dbus_set_property: default handler for the #DfsmObject::dbus-set-property signal
 * @arbitrary_transition: default handler for the #DfsmObject::arbitrary-transition signal
 *
 * Class structure for #DfsmObject.
 */
typedef struct {
	/*< private >*/
	GObjectClass parent;

	/*< public >*/
	gboolean (*dbus_method_call) (DfsmObject *obj, DfsmOutputSequence *output_sequence, const gchar *interface_name, const gchar *method_name,
	                              GVariant *parameters, gboolean enable_fuzzing);
	gboolean (*dbus_set_property) (DfsmObject *obj, DfsmOutputSequence *output_sequence, const gchar *interface_name, const gchar *property_name,
	                               GVariant *value, gboolean enable_fuzzing);
	gboolean (*arbitrary_transition) (DfsmObject *obj, DfsmOutputSequence *output_sequence, gboolean enable_fuzzing);
} DfsmObjectClass;

GType dfsm_object_get_type (void) G_GNUC_CONST;

GPtrArray *dfsm_object_factory_asts_from_data (const gchar *simulation_code, const gchar *introspection_xml,
                                               GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC; /* array of DfsmAstObjects */
GPtrArray *dfsm_object_factory_from_data (const gchar *simulation_code, const gchar *introspection_xml,
                                          GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC; /* array of DfsmObjects */

void dfsm_object_factory_from_files (GFile *simulation_code_file, GFile *introspection_xml_file, GCancellable *cancellable,
                                     GAsyncReadyCallback callback, gpointer user_data);
GPtrArray *dfsm_object_factory_from_files_finish (GAsyncResult *async_result,
                                                  GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC; /* array of DfsmObjects */

void dfsm_object_factory_set_unfuzzed_transition_limit (guint transition_limit);

void dfsm_object_register_on_bus (DfsmObject *self, GDBusConnection *connection, GAsyncReadyCallback callback, gpointer user_data);
void dfsm_object_register_on_bus_finish (DfsmObject *self, GAsyncResult *async_result, GError **error);
void dfsm_object_unregister_on_bus (DfsmObject *self);
void dfsm_object_reset (DfsmObject *self);

GDBusConnection *dfsm_object_get_connection (DfsmObject *self) G_GNUC_PURE;
DfsmMachine *dfsm_object_get_machine (DfsmObject *self) G_GNUC_PURE;
const gchar *dfsm_object_get_object_path (DfsmObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_object_get_well_known_bus_names (DfsmObject *self) G_GNUC_PURE;
guint dfsm_object_get_dbus_activity_count (DfsmObject *self);

G_END_DECLS

#endif /* !DFSM_OBJECT_H */
