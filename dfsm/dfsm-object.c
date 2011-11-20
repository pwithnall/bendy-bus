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

#include "dfsm-object.h"
#include "dfsm-ast.h"
#include "dfsm-machine.h"
#include "dfsm-parser.h"

static void dfsm_object_dispose (GObject *object);
static void dfsm_object_finalize (GObject *object);
static void dfsm_object_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dfsm_object_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void dfsm_object_dbus_signal_emission_cb (DfsmMachine *machine, const gchar *signal_name, GVariant *parameters, DfsmObject *self);
static void dfsm_object_dbus_method_call (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                                          const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data);
static GVariant *dfsm_object_dbus_get_property (GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                                                const gchar *interface_name, const gchar *property_name, GError **error, gpointer user_data);
static gboolean dfsm_object_dbus_set_property (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                                               const gchar *property_name, GVariant *value, GError **error, gpointer user_data);

static const GDBusInterfaceVTable dbus_interface_vtable = {
	dfsm_object_dbus_method_call,
	dfsm_object_dbus_get_property,
	dfsm_object_dbus_set_property,
};

struct _DfsmObjectPrivate {
	GDBusConnection *connection; /* NULL if the object isn't registered on a bus */
	DfsmMachine *machine;
	gulong signal_emission_handler;
	gchar *object_path;
	GPtrArray/*<string>*/ *interfaces;
	GArray/*<uint>*/ *registration_ids; /* IDs for all the D-Bus interface registrations we've made, in the same order as ->interfaces. */
};

enum {
	PROP_CONNECTION = 1,
	PROP_MACHINE,
	PROP_OBJECT_PATH,
	PROP_INTERFACES,
};

G_DEFINE_TYPE (DfsmObject, dfsm_object, G_TYPE_OBJECT)

static void
dfsm_object_class_init (DfsmObjectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmObjectPrivate));

	gobject_class->get_property = dfsm_object_get_property;
	gobject_class->set_property = dfsm_object_set_property;
	gobject_class->dispose = dfsm_object_dispose;
	gobject_class->finalize = dfsm_object_finalize;

	/**
	 * DfsmObject:connection:
	 *
	 * A connection to a D-Bus bus. This will be %NULL if the object isn't currently registered on a bus.
	 */
	g_object_class_install_property (gobject_class, PROP_CONNECTION,
	                                 g_param_spec_object ("connection",
	                                                      "Connection", "A connection to a D-Bus bus.",
	                                                      G_TYPE_DBUS_CONNECTION,
	                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * DfsmObject:machine:
	 *
	 * The DFSM simulation which provides the behaviour of this object.
	 */
	g_object_class_install_property (gobject_class, PROP_MACHINE,
	                                 g_param_spec_object ("machine",
	                                                      "Machine", "The DFSM simulation which provides the behaviour of this object.",
	                                                      DFSM_TYPE_MACHINE,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DfsmObject:object-path:
	 *
	 * The path of this object on the bus.
	 */
	g_object_class_install_property (gobject_class, PROP_OBJECT_PATH,
	                                 g_param_spec_string ("object-path",
	                                                      "Object path", "The path of this object on the bus.",
	                                                      NULL,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DfsmObject:interfaces:
	 *
	 * An array of the D-Bus interface names this object implements. This is a #GPtrArray containing normal C strings, each of which is a D-Bus
	 * interface name.
	 */
	g_object_class_install_property (gobject_class, PROP_INTERFACES,
	                                 g_param_spec_boxed ("interfaces",
	                                                     "Interfaces", "An array of the D-Bus interface names this object implements.",
	                                                     G_TYPE_PTR_ARRAY,
	                                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
dfsm_object_init (DfsmObject *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_OBJECT, DfsmObjectPrivate);
}

static void
dfsm_object_dispose (GObject *object)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (object)->priv;

	/* Unregister on the bus first. */
	dfsm_object_unregister_on_bus (DFSM_OBJECT (object));

	/* Free things. */
	if (priv->interfaces != NULL) {
		g_ptr_array_unref (priv->interfaces);
		priv->interfaces = NULL;
	}

	if (priv->machine != NULL) {
		g_signal_handler_disconnect (priv->machine, priv->signal_emission_handler);
		g_object_unref (priv->machine);
		priv->machine = NULL;
	}

	g_clear_object (&priv->connection);

	/* Shouldn't leak this. */
	g_assert (priv->registration_ids == NULL);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_object_parent_class)->dispose (object);
}

static void
dfsm_object_finalize (GObject *object)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (object)->priv;

	g_free (priv->object_path);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_object_parent_class)->finalize (object);
}

static void
dfsm_object_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (object)->priv;

	switch (property_id) {
		case PROP_CONNECTION:
			g_value_set_object (value, priv->connection);
			break;
		case PROP_MACHINE:
			g_value_set_object (value, priv->machine);
			break;
		case PROP_OBJECT_PATH:
			g_value_set_string (value, priv->object_path);
			break;
		case PROP_INTERFACES:
			g_value_set_boxed (value, priv->interfaces);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dfsm_object_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (object)->priv;

	switch (property_id) {
		case PROP_MACHINE:
			/* Construct-only */
			priv->machine = g_value_dup_object (value);
			priv->signal_emission_handler = g_signal_connect (priv->machine, "signal-emission",
			                                                  (GCallback) dfsm_object_dbus_signal_emission_cb, object);
			break;
		case PROP_OBJECT_PATH:
			/* Construct-only */
			priv->object_path = g_value_dup_string (value);
			break;
		case PROP_INTERFACES:
			/* Construct-only */
			priv->interfaces = g_ptr_array_ref (g_value_get_boxed (value));
			break;
		case PROP_CONNECTION:
			/* Read-only */
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/*
 * dfsm_object_new:
 * @machine: a DFSM simulation to provide the object's behaviour
 * @object_path: the object's path on the bus
 * @interfaces: the interfaces to export the object as
 *
 * Creates a new #DfsmObject with behaviour given by @machine, which will be exported as @object_path (implementing all the interfaces in @interfaces)
 * on the D-Bus connection given when dfsm_object_register_on_bus() is called.
 *
 * Return value: (transfer full): a new #DfsmObject
 */
static DfsmObject *
_dfsm_object_new (DfsmMachine *machine, const gchar *object_path, GPtrArray/*<string>*/ *interfaces)
{
	g_return_val_if_fail (DFSM_IS_MACHINE (machine), NULL);
	g_return_val_if_fail (object_path != NULL, NULL);
	g_return_val_if_fail (interfaces != NULL, NULL);

	return g_object_new (DFSM_TYPE_OBJECT,
	                     "machine", machine,
	                     "object-path", object_path,
	                     "interfaces", interfaces,
	                     NULL);
}

/**
 * dfsm_object_factory_from_files:
 * @simulation_code: code describing the DFSM of one or more D-Bus objects to be simulated
 * @introspection_xml: D-Bus introspection XML describing all the interfaces referenced by @simulation_code
 * @error: (allow-none): a #GError, or %NULL
 *
 * Parses the given @simulation_code and constructs one or more #DfsmObject<!-- -->s from it, each of which will simulate a single D-Bus object on the
 * bus once started using dfsm_object_register_on_bus(). The given @introspection_xml should be a fully formed introspection XML
 * document which, at a minimum, describes all the D-Bus interfaces implemented by all the objects defined in @simulation_code.
 *
 * Return value: (transfer full): an array of #DfsmObject<!-- -->s, each of which must be freed using g_object_unref()
 */
GPtrArray/*<DfsmObject>*/ *
dfsm_object_factory_from_files (const gchar *simulation_code, const gchar *introspection_xml, GError **error)
{
	GPtrArray/*<DfsmAstObject>*/ *ast_object_array;
	GPtrArray/*<DfsmObject>*/ *object_array;
	guint i;
	GDBusNodeInfo *dbus_node_info;
	GError *child_error = NULL;

	g_return_val_if_fail (simulation_code != NULL, NULL);
	g_return_val_if_fail (introspection_xml != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Load the D-Bus interface introspection info. */
	dbus_node_info = g_dbus_node_info_new_for_xml (introspection_xml, &child_error);

	if (child_error != NULL) {
		/* Error! */
		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Parse the source code to get an array of ASTs. */
	ast_object_array = dfsm_bison_parse (dbus_node_info, simulation_code, &child_error);

	g_dbus_node_info_unref (dbus_node_info);

	/* For each of the AST objects, build a proper DfsmObject. */
	object_array = g_ptr_array_new_with_free_func (g_object_unref);

	for (i = 0; i < ast_object_array->len; i++) {
		DfsmAstObject *ast_object;
		DfsmMachine *machine;
		DfsmObject *object;

		ast_object = g_ptr_array_index (ast_object_array, i);

		/* Build the machine and object wrapper. */
		machine = _dfsm_machine_new (ast_object->environment, ast_object->states, ast_object->transitions);
		object = _dfsm_object_new (machine, ast_object->object_path, ast_object->interface_names);

		g_ptr_array_add (object_array, g_object_ref (object));

		g_object_unref (object);
		g_object_unref (machine);
	}

	g_ptr_array_unref (ast_object_array);

	return object_array;
}

static void
dfsm_object_dbus_signal_emission_cb (DfsmMachine *machine, const gchar *signal_name, GVariant *parameters, DfsmObject *self)
{
	DfsmObjectPrivate *priv;
	GDBusNodeInfo *dbus_node_info;
	GDBusInterfaceInfo **interfaces;
	const gchar *interface_name = NULL;
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_OBJECT (self));

	priv = self->priv;

	/* This should only ever get called while the simulation's running. */
	g_assert (priv->registration_ids != NULL);

	/* Find the name of the interface defining the signal. TODO: This is slow. Is there no better way? */
	dbus_node_info = dfsm_environment_get_dbus_node_info (dfsm_machine_get_environment (priv->machine));

	for (interfaces = dbus_node_info->interfaces; *interfaces != NULL; interfaces++) {
		if (g_dbus_interface_info_lookup_signal (*interfaces, signal_name) != NULL) {
			/* Found the interface. */
			interface_name = (*interfaces)->name;
			break;
		}
	}

	if (interface_name == NULL) {
		g_warning ("Runtime error in simulation: Couldn't find interface containing signal ‘%s’.", signal_name);
		return;
	}

	/* Emit the signal. */
	g_dbus_connection_emit_signal (priv->connection, NULL, priv->object_path, interface_name, signal_name, parameters, &child_error);

	if (child_error != NULL) {
		g_warning ("Runtime error in simulation while emitting D-Bus signal ‘%s’: %s", signal_name, child_error->message);
		g_clear_error (&child_error);
	}
}

static void
dfsm_object_dbus_method_call (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                              const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (user_data)->priv;
	GVariant *return_value;
	GError *child_error = NULL;

	/* Pass the method call through to the DFSM. */
	return_value = dfsm_machine_call_method (priv->machine, method_name, parameters, &child_error);

	if (child_error != NULL) {
		/* Error. It's either a runtime error from within the simulator, or a thrown D-Bus error from user code. */
		if (g_dbus_error_is_remote_error (child_error) == TRUE) {
			/* Thrown D-Bus error. */
			g_dbus_method_invocation_return_gerror (invocation, child_error);
		} else {
			/* Runtime error. Replace it with a generic D-Bus error so as not to expose internals of the
			 * simulator to programs under test. */
			g_warning ("Runtime error in simulation while handling D-Bus method call ‘%s’: %s", method_name, child_error->message);
			g_dbus_method_invocation_return_dbus_error (invocation, "org.freedesktop.DBus.Error.Failed", child_error->message);
		}

		g_clear_error (&child_error);
	} else if (return_value != NULL) {
		/* Success! Return this value as a reply. */
		g_dbus_method_invocation_return_value (invocation, return_value);
		g_variant_unref (return_value);
	} else {
		/* Success, but no value to return. */
		g_dbus_method_invocation_return_value (invocation, NULL);
	}
}

static GVariant *
dfsm_object_dbus_get_property (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                               const gchar *property_name, GError **error, gpointer user_data)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (user_data)->priv;
	GVariant *value;

	/* Grab the value from the environment and be done with it. */
	value = dfsm_environment_dup_variable_value (dfsm_machine_get_environment (priv->machine), DFSM_VARIABLE_SCOPE_OBJECT, property_name);

	if (value == NULL) {
		/* Variable wasn't found. This shouldn't ever happen, since it's checked for in the checking stage of interpretation. */
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Runtime error in simulation: Variable ‘%s’ could not be found.", property_name);
	}

	return value;
}

static gboolean
dfsm_object_dbus_set_property (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                               const gchar *property_name, GVariant *value, GError **error, gpointer user_data)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (user_data)->priv;
	DfsmEnvironment *environment;
	GVariant *old_value;
	GVariantBuilder *builder;
	GError *child_error = NULL;

	/* Check to see if the value's actually changed. If it hasn't, bail. */
	environment = dfsm_machine_get_environment (priv->machine);
	old_value = dfsm_environment_dup_variable_value (environment, DFSM_VARIABLE_SCOPE_OBJECT, property_name);

	if (old_value != NULL && g_variant_equal (old_value, value) == TRUE) {
		g_variant_unref (old_value);
		return TRUE;
	}

	g_variant_unref (old_value);

	/* Set the variable's new value in the environment. */
	dfsm_environment_set_variable_value (environment, DFSM_VARIABLE_SCOPE_OBJECT, property_name, value);

	/* Emit a notification. */
	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (builder, "{sv}", property_name, value);
	g_dbus_connection_emit_signal (connection, NULL, object_path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
	                               g_variant_new ("(sa{sv}as)", interface_name, builder, NULL), &child_error);

	if (child_error != NULL) {
		g_warning ("Runtime error in simulation while notifying of update to D-Bus property ‘%s’: %s", property_name, child_error->message);
		g_clear_error (&child_error);
	}

	return TRUE;
}

/**
 * dfsm_object_register_on_bus:
 * @self: a #DfsmObject
 * @error: (allow-none): a #GError, or %NULL
 *
 * Register this simulated D-Bus object on the D-Bus instance given by @connection, and then start the simulation. Once the simulation is started
 * successfully, the object will respond to method calls on the bus as directed by its DFSM (#DfsmObject:machine), and will also take arbitrary actions
 * at random intervals (also as directed by its DFSM).
 *
 * If the object is successfully registered on the bus, #DfsmObject:connection will be set to @connection.
 *
 * If the object is already registered on the bus, this function will return immediately. The object can be unregistered from the bus by calling
 * dfsm_object_unregister_on_bus().
 */
void
dfsm_object_register_on_bus (DfsmObject *self, GDBusConnection *connection, GError **error)
{
	DfsmObjectPrivate *priv;
	guint i;
	GDBusNodeInfo *dbus_node_info;
	GArray *registration_ids;
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_OBJECT (self));
	g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
	g_return_if_fail (error == NULL || *error == NULL);

	priv = self->priv;

	/* Bail if we're already registered. */
	if (priv->registration_ids != NULL) {
		return;
	}

	dbus_node_info = dfsm_environment_get_dbus_node_info (dfsm_machine_get_environment (priv->machine));
	registration_ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), priv->interfaces->len);

	/* Register on the bus for each of the interfaces we implement. */
	for (i = 0; i < priv->interfaces->len; i++) {
		const gchar *interface_name;
		GDBusInterfaceInfo *interface_info;
		guint registration_id;

		interface_name = g_ptr_array_index (priv->interfaces, i);

		/* Look up the interface information. */
		interface_info = g_dbus_node_info_lookup_interface (dbus_node_info, interface_name);

		if (interface_info == NULL) {
			/* Unknown interface! */
			g_set_error (&child_error, DFSM_SIMULATION_ERROR, DFSM_SIMULATION_ERROR_UNKNOWN_INTERFACE,
			             "D-Bus interface ‘%s’ not found in introspection XML.", interface_name);
			goto error;
		}

		/* Register on the bus. */
		registration_id = g_dbus_connection_register_object (connection, priv->object_path, interface_info, &dbus_interface_vtable, self,
		                                                     NULL, &child_error);

		if (child_error != NULL) {
			/* Error */
			goto error;
		}

		/* Success! Add the registration_id to our list of successfully-registered interfaces. */
		g_array_append_val (registration_ids, registration_id);
	}

	/* Success! Save the array of registration IDs so that we can unregister later. */
	priv->registration_ids = registration_ids;

	/* Expose the connection. */
	g_assert (priv->connection == NULL);
	priv->connection = g_object_ref (connection);
	g_object_notify (G_OBJECT (self), "connection");

	/* Start the DFSM. */
	dfsm_machine_start_simulation (priv->machine);

	return;

error:
	g_assert (child_error != NULL);

	/* Unregister all the interfaces we successfully registered before encountering the error. */
	while (i-- > 0) {
		guint registration_id = g_array_index (registration_ids, guint, i);
		g_dbus_connection_unregister_object (connection, registration_id);
	}

	g_array_free (registration_ids, TRUE);

	/* Propagate the error and return. */
	g_propagate_error (error, child_error);
}

/**
 * dfsm_object_unregister_on_bus:
 * @self: a #DfsmObject
 *
 * Unregister this simulated D-Bus object on the D-Bus instance at #DfsmObject:connection, after stopping the simulation. Once the object has been
 * removed from the bus it will no longer respond to method calls on the bus, and will no longer take arbitrary actions at random intervals (as
 * directed by its DFSM).
 *
 * If the object is successfully unregistered, #DfsmObject:connection will be set to %NULL.
 *
 * If the object is already unregistered on the bus, this function will return immediately. The object can be registered on the bus by calling
 * dfsm_object_register_on_bus().
 */
void
dfsm_object_unregister_on_bus (DfsmObject *self)
{
	DfsmObjectPrivate *priv;
	guint i;

	g_return_if_fail (DFSM_IS_OBJECT (self));

	priv = self->priv;

	/* Bail if we're already unregistered. */
	if (priv->registration_ids == NULL) {
		return;
	}

	/* Stop the DFSM. */
	dfsm_machine_stop_simulation (priv->machine);

	/* Unregister all the interfaces from the bus. */
	for (i = 0; i < priv->registration_ids->len; i++) {
		guint registration_id = g_array_index (priv->registration_ids, guint, i);
		g_dbus_connection_unregister_object (priv->connection, registration_id);
	}

	g_array_free (priv->registration_ids, TRUE);
	priv->registration_ids = NULL;

	g_clear_object (&priv->connection);
	g_object_notify (G_OBJECT (self), "connection");
}

/**
 * dfsm_object_get_connection:
 * @self: a #DfsmMachine
 *
 * Gets the value of the #DfsmObject:connection property. This will be %NULL if the object isn't currently registered on a bus.
 *
 * Return value: (transfer none) (allow-none): the object's connection to the bus, or %NULL
 */
GDBusConnection *
dfsm_object_get_connection (DfsmObject *self)
{
	g_return_val_if_fail (DFSM_IS_OBJECT (self), NULL);

	return self->priv->connection;
}

/**
 * dfsm_object_get_machine:
 * @self: a #DfsmMachine
 *
 * Gets the value of the #DfsmObject:machine property.
 *
 * Return value: (transfer none): the simulation which provides the behaviour of this object
 */
DfsmMachine *
dfsm_object_get_machine (DfsmObject *self)
{
	g_return_val_if_fail (DFSM_IS_OBJECT (self), NULL);

	return self->priv->machine;
}

/**
 * dfsm_object_get_object_path:
 * @self: a #DfsmObject
 *
 * Gets the value of the #DfsmObject:object-path property.
 *
 * Return value: (transfer none): the path of this object on the bus
 */
const gchar *
dfsm_object_get_object_path (DfsmObject *self)
{
	g_return_val_if_fail (DFSM_IS_OBJECT (self), NULL);

	return self->priv->object_path;
}
