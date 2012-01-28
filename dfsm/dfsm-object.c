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

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-object.h"
#include "dfsm-ast.h"
#include "dfsm-dbus-output-sequence.h"
#include "dfsm-machine.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

GType
dfsm_simulation_status_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ DFSM_SIMULATION_STATUS_STOPPED, "DFSM_SIMULATION_STATUS_STOPPED", "stopped" },
			{ DFSM_SIMULATION_STATUS_STARTED, "DFSM_SIMULATION_STATUS_STARTED", "started" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("DfsmSimulationStatus", values);
	}

	return etype;
}

/* Arbitrarily-chosen min. and max. values for the arbitrary transition timeout callbacks. */
#define MIN_TIMEOUT 50 /* ms */
#define MAX_TIMEOUT 200 /* ms */

static void dfsm_object_dispose (GObject *object);
static void dfsm_object_finalize (GObject *object);
static void dfsm_object_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dfsm_object_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

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
	DfsmSimulationStatus simulation_status;
	guint timeout_id;
	gchar *object_path;
	GPtrArray/*<string>*/ *bus_names;
	GPtrArray/*<string>*/ *interfaces;
	GArray/*<uint>*/ *registration_ids; /* IDs for all the D-Bus interface registrations we've made, in the same order as ->interfaces. */
	GHashTable/*<string, uint>*/ *bus_name_ids; /* map from well-known bus name to its ownership ID */
	guint outstanding_bus_ownership_callbacks; /* number of calls to g_bus_own_name() which are outstanding */
	guint dbus_activity_count;
};

/* HACK: Apply to all DfsmObjects. Not thread-safe. */
static guint unfuzzed_transition_count = 0;
static guint unfuzzed_transition_limit = 0;

enum {
	PROP_CONNECTION = 1,
	PROP_MACHINE,
	PROP_OBJECT_PATH,
	PROP_WELL_KNOWN_BUS_NAMES,
	PROP_INTERFACES,
	PROP_DBUS_ACTIVITY_COUNT,
	PROP_SIMULATION_STATUS,
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
	 * DfsmObject:well-known-bus-names:
	 *
	 * An array of the well-known D-Bus bus names this object should own. This is a #GPtrArray containing normal C strings, each of which is a D-Bus
	 * well-known bus name.
	 */
	g_object_class_install_property (gobject_class, PROP_WELL_KNOWN_BUS_NAMES,
	                                 g_param_spec_boxed ("well-known-bus-names",
	                                                     "Well-known bus names",
	                                                     "An array of the well-known D-Bus bus names this object should own.",
	                                                     G_TYPE_PTR_ARRAY,
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

	/**
	 * DfsmObject:dbus-activity-count:
	 *
	 * A count of the number of D-Bus activities which have occurred. An activity is a method call to a method on this object, getting one of this
	 * object's properties, or setting one of them. Counting starts when dfsm_object_register_on_bus() is called and stops when
	 * dfsm_object_unregister_on_bus() is called (but the count isn't reset until the next call to dfsm_object_register_on_bus() or
	 * dfsm_object_reset()).
	 */
	g_object_class_install_property (gobject_class, PROP_DBUS_ACTIVITY_COUNT,
	                                 g_param_spec_uint ("dbus-activity-count",
	                                                    "D-Bus activity count", "A count of the number of D-Bus activities which have occurred.",
	                                                    0, G_MAXUINT, 0,
	                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * DfsmObject:simulation-status:
	 *
	 * The status of the simulation; i.e. whether it's currently started or stopped.
	 */
	g_object_class_install_property (gobject_class, PROP_SIMULATION_STATUS,
	                                 g_param_spec_enum ("simulation-status",
	                                                    "Simulation status", "The status of the simulation.",
	                                                    DFSM_TYPE_SIMULATION_STATUS, DFSM_SIMULATION_STATUS_STOPPED,
	                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
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

	if (priv->bus_names != NULL) {
		g_ptr_array_unref (priv->bus_names);
		priv->bus_names = NULL;
	}

	if (priv->machine != NULL) {
		g_object_unref (priv->machine);
		priv->machine = NULL;
	}

	g_clear_object (&priv->connection);

	/* Shouldn't leak these. */
	g_assert (priv->registration_ids == NULL);
	g_assert (priv->bus_name_ids == NULL);

	/* Make sure we're not leaking a callback. */
	g_assert (priv->timeout_id == 0);

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
		case PROP_WELL_KNOWN_BUS_NAMES:
			g_value_set_boxed (value, priv->bus_names);
			break;
		case PROP_INTERFACES:
			g_value_set_boxed (value, priv->interfaces);
			break;
		case PROP_DBUS_ACTIVITY_COUNT:
			g_value_set_uint (value, priv->dbus_activity_count);
			break;
		case PROP_SIMULATION_STATUS:
			g_value_set_enum (value, priv->simulation_status);
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
			break;
		case PROP_OBJECT_PATH:
			/* Construct-only */
			priv->object_path = g_value_dup_string (value);
			break;
		case PROP_WELL_KNOWN_BUS_NAMES:
			/* Construct-only */
			priv->bus_names = g_ptr_array_ref (g_value_get_boxed (value));
			break;
		case PROP_INTERFACES:
			/* Construct-only */
			priv->interfaces = g_ptr_array_ref (g_value_get_boxed (value));
			break;
		case PROP_CONNECTION:
			/* Read-only */
		case PROP_DBUS_ACTIVITY_COUNT:
			/* Read-only */
		case PROP_SIMULATION_STATUS:
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
 * @bus_names: an array of well-known bus names for the object to own
 * @interfaces: the interfaces to export the object as
 *
 * Creates a new #DfsmObject with behaviour given by @machine, which will be exported as @object_path (implementing all the interfaces in @interfaces)
 * on the D-Bus connection given when dfsm_object_register_on_bus() is called. The process will take ownership of all well-known bus names in @bus_names
 * when creating the object.
 *
 * Return value: (transfer full): a new #DfsmObject
 */
static DfsmObject *
_dfsm_object_new (DfsmMachine *machine, const gchar *object_path, GPtrArray/*<string>*/ *bus_names, GPtrArray/*<string>*/ *interfaces)
{
	g_return_val_if_fail (DFSM_IS_MACHINE (machine), NULL);
	g_return_val_if_fail (object_path != NULL, NULL);
	g_return_val_if_fail (bus_names != NULL, NULL);
	g_return_val_if_fail (interfaces != NULL, NULL);

	return g_object_new (DFSM_TYPE_OBJECT,
	                     "machine", machine,
	                     "object-path", object_path,
	                     "well-known-bus-names", bus_names,
	                     "interfaces", interfaces,
	                     NULL);
}

/**
 * dfsm_object_factory_asts_from_files:
 * @simulation_code: code describing the DFSM of one or more D-Bus objects to be simulated
 * @introspection_xml: D-Bus introspection XML describing all the interfaces referenced by @simulation_code
 * @error: (allow-none): a #GError, or %NULL
 *
 * Parses the given @simulation_code and constructs one or more #DfsmAstObject<!-- -->s from it, each of which is the AST of the code representing that
 * simulated D-Bus object. The given @introspection_xml should be a fully formed introspection XML document which, at a minimum, describes all the
 * D-Bus interfaces implemented by all the objects defined in @simulation_code.
 *
 * Return value: (transfer full): an array of #DfsmAstObject<!-- -->s, each of which must be freed using g_object_unref()
 */
GPtrArray/*<DfsmAstObject>*/ *
dfsm_object_factory_asts_from_files (const gchar *simulation_code, const gchar *introspection_xml, GError **error)
{
	GPtrArray/*<DfsmAstObject>*/ *ast_object_array;
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

	if (child_error != NULL) {
		/* Error! */
		g_propagate_error (error, child_error);

		return NULL;
	}

	/* Check all the objects. */
	for (i = 0; i < ast_object_array->len; i++) {
		DfsmAstObject *ast_object;

		ast_object = g_ptr_array_index (ast_object_array, i);

		dfsm_ast_object_initial_check (ast_object, &child_error);

		if (child_error != NULL) {
			/* Error! */
			g_propagate_error (error, child_error);

			g_ptr_array_unref (ast_object_array);

			return NULL;
		}
	}

	for (i = 0; i < ast_object_array->len; i++) {
		DfsmAstObject *ast_object;

		ast_object = g_ptr_array_index (ast_object_array, i);

		dfsm_ast_node_check (DFSM_AST_NODE (ast_object), dfsm_ast_object_get_environment (ast_object), &child_error);

		if (child_error != NULL) {
			/* Error! */
			g_propagate_error (error, child_error);

			g_ptr_array_unref (ast_object_array);

			return NULL;
		}
	}

	return ast_object_array;
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
	GError *child_error = NULL;

	g_return_val_if_fail (simulation_code != NULL, NULL);
	g_return_val_if_fail (introspection_xml != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Load and parse the files into DfsmAstObjects. */
	ast_object_array = dfsm_object_factory_asts_from_files (simulation_code, introspection_xml, &child_error);

	if (child_error != NULL) {
		/* Error! */
		g_propagate_error (error, child_error);

		return NULL;
	}

	/* For each of the AST objects, build a proper DfsmObject. */
	object_array = g_ptr_array_new_with_free_func (g_object_unref);

	for (i = 0; i < ast_object_array->len; i++) {
		DfsmAstObject *ast_object;
		DfsmMachine *machine;
		DfsmObject *object;

		ast_object = g_ptr_array_index (ast_object_array, i);

		/* Build the machine and object wrapper. */
		machine = _dfsm_machine_new (dfsm_ast_object_get_environment (ast_object), dfsm_ast_object_get_state_names (ast_object),
		                             dfsm_ast_object_get_transitions (ast_object));
		object = _dfsm_object_new (machine, dfsm_ast_object_get_object_path (ast_object), dfsm_ast_object_get_well_known_bus_names (ast_object),
		                           dfsm_ast_object_get_interface_names (ast_object));

		g_ptr_array_add (object_array, g_object_ref (object));

		g_object_unref (object);
		g_object_unref (machine);
	}

	g_ptr_array_unref (ast_object_array);

	return object_array;
}

/**
 * dfsm_object_factory_set_unfuzzed_transition_limit:
 * @transition_limit: number of unfuzzed transitions to execute before enabling fuzzing and error-throwing transitions
 *
 * Set the number of unfuzzed transitions to execute before enabling fuzzing and error-throwing transitions, across all #DfsmObject<!-- -->s as an
 * aggregate. May be zero.
 */
void
dfsm_object_factory_set_unfuzzed_transition_limit (guint transition_limit)
{
	unfuzzed_transition_count = 0;
	unfuzzed_transition_limit = transition_limit;
}

static void
dfsm_object_dbus_method_call (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                              const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (user_data)->priv;
	gchar *parameters_string;
	DfsmOutputSequence *output_sequence;
	GError *child_error = NULL;

	/* Debug output. */
	parameters_string = g_variant_print (parameters, FALSE);
	g_debug ("Method call from ‘%s’ to method ‘%s’ of interface ‘%s’ on object ‘%s’. Parameters: %s", sender, method_name, interface_name,
	         object_path, parameters_string);
	g_free (parameters_string);

	/* Count the activity. */
	priv->dbus_activity_count++;
	g_object_notify (G_OBJECT (user_data), "dbus-activity-count");

	/* Pass the method call through to the DFSM. */
	output_sequence = DFSM_OUTPUT_SEQUENCE (dfsm_dbus_output_sequence_new (connection, object_path, invocation));
	dfsm_machine_call_method (priv->machine, output_sequence, interface_name, method_name, parameters,
	                          (unfuzzed_transition_count >= unfuzzed_transition_limit) ? TRUE : FALSE);

	if (unfuzzed_transition_count < unfuzzed_transition_limit) {
		unfuzzed_transition_count++;
	}

	/* Output the effect sequence resulting from the method call. */
	dfsm_output_sequence_output (output_sequence, &child_error);

	g_object_unref (output_sequence);

	if (child_error != NULL) {
		/* Runtime error. Replace it with a generic D-Bus error so as not to expose internals of the
		 * simulator to programs under test. */
		g_warning (_("Runtime error in simulation while handling D-Bus method call ‘%s’: %s"), method_name, child_error->message);
		g_dbus_method_invocation_return_dbus_error (invocation, "org.freedesktop.DBus.Error.Failed", child_error->message);

		g_clear_error (&child_error);
	}
}

static GVariant *
dfsm_object_dbus_get_property (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                               const gchar *property_name, GError **error, gpointer user_data)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (user_data)->priv;
	GVariant *value;
	gchar *value_string;

	/* Count the activity. */
	priv->dbus_activity_count++;
	g_object_notify (G_OBJECT (user_data), "dbus-activity-count");

	/* Grab the value from the environment and be done with it. */
	value = dfsm_environment_dup_variable_value (dfsm_machine_get_environment (priv->machine), DFSM_VARIABLE_SCOPE_OBJECT, property_name);

	value_string = (value != NULL) ? g_variant_print (value, FALSE) : g_strdup ("(null)");
	g_debug ("Getting D-Bus property ‘%s’ of interface ‘%s’ on object ‘%s’ for sender ‘%s’, value: %s", property_name, interface_name, object_path,
	         sender, value_string);
	g_free (value_string);

	if (value == NULL) {
		/* Variable wasn't found. This shouldn't ever happen, since it's checked for in the checking stage of interpretation. */
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Runtime error in simulation: Variable ‘%s’ could not be found."), property_name);
	}

	return value;
}

static gboolean
dfsm_object_dbus_set_property (GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name,
                               const gchar *property_name, GVariant *value, GError **error, gpointer user_data)
{
	DfsmObjectPrivate *priv = DFSM_OBJECT (user_data)->priv;
	gchar *value_string;
	DfsmOutputSequence *output_sequence;
	GError *child_error = NULL;

	value_string = g_variant_print (value, FALSE);
	g_debug ("Setting D-Bus property ‘%s’ of interface ‘%s’ on object ‘%s’ for sender ‘%s’ to value: %s", property_name, interface_name,
	         object_path, sender, value_string);
	g_free (value_string);

	/* Count the activity. */
	priv->dbus_activity_count++;
	g_object_notify (G_OBJECT (user_data), "dbus-activity-count");

	/* Set the property on the machine. */
	output_sequence = DFSM_OUTPUT_SEQUENCE (dfsm_dbus_output_sequence_new (connection, object_path, NULL));

	if (dfsm_machine_set_property (priv->machine, output_sequence, interface_name, property_name, value,
	                               (unfuzzed_transition_count >= unfuzzed_transition_limit) ? TRUE : FALSE) == TRUE) {
		GVariantBuilder *builder;
		GVariant *parameters;

		/* Schedule a notification. */
		builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
		g_variant_builder_add (builder, "{sv}", property_name, value);
		parameters = g_variant_new ("(sa{sv}as)", interface_name, builder, NULL);

		dfsm_output_sequence_add_emit (output_sequence, "org.freedesktop.DBus.Properties", "PropertiesChanged", parameters);

		g_variant_unref (parameters);
	}

	if (unfuzzed_transition_count < unfuzzed_transition_limit) {
		unfuzzed_transition_count++;
	}

	/* Output effects of the transition. */
	dfsm_output_sequence_output (output_sequence, &child_error);

	g_object_unref (output_sequence);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return FALSE;
	}

	return TRUE;
}

static void schedule_arbitrary_transition (DfsmObject *self);

/* This gets called continuously at random intervals while the simulation's running. It checks whether any arbitrary transitions can be taken,
 * and if so, follows one of them. */
static gboolean
arbitrary_transition_timeout_cb (DfsmObject *self)
{
	DfsmObjectPrivate *priv = self->priv;
	DfsmOutputSequence *output_sequence;
	GError *child_error = NULL;

	/* Make an arbitrary transition. */
	output_sequence = DFSM_OUTPUT_SEQUENCE (dfsm_dbus_output_sequence_new (priv->connection, priv->object_path, NULL));
	dfsm_machine_make_arbitrary_transition (self->priv->machine, output_sequence,
	                                        (unfuzzed_transition_count >= unfuzzed_transition_limit) ? TRUE : FALSE);

	if (unfuzzed_transition_count < unfuzzed_transition_limit) {
		unfuzzed_transition_count++;
	}

	/* Output the transition's effects. */
	dfsm_output_sequence_output (output_sequence, &child_error);

	g_object_unref (output_sequence);

	if (child_error != NULL) {
		g_warning ("Runtime error when outputting the effects of an arbitrary transition: %s", child_error->message);
		g_error_free (child_error);
	}

	/* Schedule the next arbitrary transition. */
	priv->timeout_id = 0;
	schedule_arbitrary_transition (self);

	return FALSE; /* rescheduled with a different timeout above */
}

static void
schedule_arbitrary_transition (DfsmObject *self)
{
	guint32 timeout_period;

	g_assert (self->priv->timeout_id == 0);

	/* Add a random timeout to the next potential arbitrary transition. */
	timeout_period = g_random_int_range (MIN_TIMEOUT, MAX_TIMEOUT);
	g_debug ("Scheduling the next arbitrary transition in %u ms.", timeout_period);
	self->priv->timeout_id = g_timeout_add (timeout_period, (GSourceFunc) arbitrary_transition_timeout_cb, self);
}

static void
start_simulation (DfsmObject *self, GDBusConnection *connection, GSimpleAsyncResult *async_result)
{
	DfsmObjectPrivate *priv = self->priv;

	/* Expose the connection. */
	g_assert (priv->connection == NULL);
	priv->connection = g_object_ref (connection);
	g_object_notify (G_OBJECT (self), "connection");

	/* Reset the activity counter. */
	priv->dbus_activity_count = 0;
	g_object_notify (G_OBJECT (self), "dbus-activity-count");

	unfuzzed_transition_count = 0;

	/* Start the DFSM. */
	g_debug ("Starting the simulation. %u unfuzzed transitions to go.", unfuzzed_transition_limit);

	/* Add a random timeout to the next potential arbitrary transition. */
	schedule_arbitrary_transition (self);

	/* Change simulation status. */
	priv->simulation_status = DFSM_SIMULATION_STATUS_STARTED;
	g_object_notify (G_OBJECT (self), "simulation-status");

	/* Return in the async result. */
	g_simple_async_result_complete_in_idle (async_result);
}

static void
name_acquired_cb (GDBusConnection *connection, const gchar *name, GSimpleAsyncResult *async_result)
{
	DfsmObject *self;
	DfsmObjectPrivate *priv;

	self = DFSM_OBJECT (g_async_result_get_source_object (G_ASYNC_RESULT (async_result)));
	priv = self->priv;

	g_debug ("Acquired ownership of well-known bus name: %s", name);

	/* Bail if this isn't the last callback. */
	priv->outstanding_bus_ownership_callbacks--;
	if (priv->outstanding_bus_ownership_callbacks > 0) {
		return;
	}

	/* Start the simulation! */
	start_simulation (self, connection, async_result);
}

static void
name_lost_cb (GDBusConnection *connection, const gchar *name, GAsyncResult *async_result)
{
	g_debug ("Lost ownership of well-known bus name: %s", name);
}

/**
 * dfsm_object_register_on_bus:
 * @self: a #DfsmObject
 * @connection: a #GDBusConnection to register the object on
 * @callback: a function to call once the asynchronous registration operation is complete
 * @user_data: (allow-none): user data to pass to @callback, or %NULL
 *
 * Register this simulated D-Bus object on the D-Bus instance given by @connection, and then start the simulation. Once the simulation is started
 * successfully, the object will respond to method calls on the bus as directed by its DFSM (#DfsmObject:machine), and will also take arbitrary actions
 * at random intervals (also as directed by its DFSM).
 *
 * If the object is successfully registered on the bus, #DfsmObject:connection will be set to @connection.
 *
 * If the object is already registered on the bus, this function will return (via @callback) immediately. The object can be unregistered from the bus
 * by calling dfsm_object_unregister_on_bus().
 *
 * Call dfsm_object_register_on_bus_finish() from @callback to handle results of this asynchronous operation, such as errors.
 */
void
dfsm_object_register_on_bus (DfsmObject *self, GDBusConnection *connection, GAsyncReadyCallback callback, gpointer user_data)
{
	DfsmObjectPrivate *priv;
	guint i;
	GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;
	GArray *registration_ids;
	GPtrArray/*<string>*/ *bus_names = NULL;
	GHashTable/*<string, uint> */ *bus_name_ids = NULL;
	GSimpleAsyncResult *async_result;
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_OBJECT (self));
	g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
	g_return_if_fail (callback != NULL);

	priv = self->priv;

	async_result = g_simple_async_result_new (G_OBJECT (self), callback, user_data, dfsm_object_register_on_bus);

	/* Bail if we're already registered. */
	g_assert ((priv->registration_ids == NULL) == (priv->bus_name_ids == NULL));

	if (priv->registration_ids != NULL) {
		g_simple_async_result_complete_in_idle (async_result);
		g_object_unref (async_result);
		return;
	}

	interfaces = dfsm_environment_get_interfaces (dfsm_machine_get_environment (priv->machine));
	registration_ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), priv->interfaces->len);

	/* Register on the bus for each of the interfaces we implement. */
	for (i = 0; i < priv->interfaces->len; i++) {
		const gchar *interface_name;
		GDBusInterfaceInfo *interface_info = NULL;
		guint registration_id, j;

		interface_name = g_ptr_array_index (priv->interfaces, i);

		/* Look up the interface information. This should always be present, since dfsm_environment_get_interfaces() is instantiated from
		 * priv->interfaces. */
		for (j = 0; j < interfaces->len; j++) {
			if (strcmp (interface_name, ((GDBusInterfaceInfo*) g_ptr_array_index (interfaces, j))->name) == 0) {
				interface_info = (GDBusInterfaceInfo*) g_ptr_array_index (interfaces, j);
				break;
			}
		}

		g_assert (interface_info != NULL);

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

	/* Register the process for all the object's well-known names. */
	bus_names = dfsm_object_get_well_known_bus_names (self);

	/* Hold an outstanding callback while we loop over the bus names, so that don't spawn the program under test before we've finished
	 * requesting to own all our bus names (e.g. if their callbacks are called very quickly. */
	priv->outstanding_bus_ownership_callbacks++;
	bus_name_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (i = 0; i < bus_names->len; i++) {
		const gchar *bus_name;
		guint bus_name_id;

		bus_name = g_ptr_array_index (bus_names, i);

		/* Skip the name if another object's already requested to own it. */
		if (g_hash_table_lookup_extended (bus_name_ids, bus_name, NULL, NULL) == TRUE) {
			continue;
		}

		/* Own the name. We keep a count of all the outstanding callbacks and only start the simulation once all are complete. */
		priv->outstanding_bus_ownership_callbacks++;
		bus_name_id = g_bus_own_name_on_connection (connection, bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
		                                            (GBusNameAcquiredCallback) name_acquired_cb, (GBusNameLostCallback) name_lost_cb,
		                                            g_object_ref (async_result), g_object_unref);
		g_hash_table_insert (bus_name_ids, g_strdup (bus_name), GUINT_TO_POINTER (bus_name_id));
	}

	/* Release our outstanding callback and start the simulation if it hasn't been started already. */
	priv->outstanding_bus_ownership_callbacks--;
	if (priv->outstanding_bus_ownership_callbacks == 0) {
		start_simulation (self, connection, async_result);
	}

	/* Success! Save the array of bus name IDs so we can unown them later. */
	priv->bus_name_ids = bus_name_ids;

	g_object_unref (async_result);

	return;

error:
	g_assert (child_error != NULL);
	g_assert (bus_names == NULL);

	/* Unregister all the interfaces we successfully registered before encountering the error. */
	while (i-- > 0) {
		guint registration_id = g_array_index (registration_ids, guint, i);
		g_dbus_connection_unregister_object (connection, registration_id);
	}

	g_array_free (registration_ids, TRUE);

	/* Propagate the error and return. */
	g_simple_async_result_take_error (async_result, child_error);
	g_simple_async_result_complete_in_idle (async_result);

	g_object_unref (async_result);
}

/**
 * dfsm_object_register_on_bus_finish:
 * @self: a #DfsmObject
 * @async_result: the asynchronous result passed to the callback
 * @error: (allow-none): a #GError, or %NULL
 *
 * Finish an asynchronous registration operation started by dfsm_object_register_on_bus().
 */
void
dfsm_object_register_on_bus_finish (DfsmObject *self, GAsyncResult *async_result, GError **error)
{
	g_return_if_fail (DFSM_IS_OBJECT (self));
	g_return_if_fail (G_IS_ASYNC_RESULT (async_result));
	g_return_if_fail (error == NULL || *error == NULL);
	g_return_if_fail (g_simple_async_result_is_valid (async_result, G_OBJECT (self), dfsm_object_register_on_bus));

	g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (async_result), error);
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
	GHashTableIter iter;
	guint i;
	gpointer bus_name_id_ptr;

	g_return_if_fail (DFSM_IS_OBJECT (self));

	priv = self->priv;

	/* Bail if we're already unregistered. */
	g_assert ((priv->registration_ids == NULL) == (priv->bus_name_ids == NULL));

	if (priv->registration_ids == NULL) {
		return;
	}

	/* Stop the DFSM. */
	g_debug ("Stopping the simulation.");

	/* Cancel any outstanding potential arbitrary transition. */
	g_debug ("Cancelling outstanding arbitrary transitions.");
	g_source_remove (priv->timeout_id);
	priv->timeout_id = 0;

	/* Change simulation status. */
	priv->simulation_status = DFSM_SIMULATION_STATUS_STOPPED;
	g_object_notify (G_OBJECT (self), "simulation-status");

	/* Unregister the well-known names. */
	g_hash_table_iter_init (&iter, priv->bus_name_ids);

	while (g_hash_table_iter_next (&iter, NULL, &bus_name_id_ptr) == TRUE) {
		g_bus_unown_name (GPOINTER_TO_UINT (bus_name_id_ptr));
	}

	g_hash_table_unref (priv->bus_name_ids);
	priv->bus_name_ids = NULL;

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
 * dfsm_object_reset:
 * @self: a #DfsmObject
 *
 * Resets the state of the #DfsmObject simulation to its initial state, as if it had just been created. Any bus registrations are retained.
 */
void
dfsm_object_reset (DfsmObject *self)
{
	DfsmObjectPrivate *priv;

	g_return_if_fail (DFSM_IS_OBJECT (self));

	priv = self->priv;

	if (priv->simulation_status == DFSM_SIMULATION_STATUS_STARTED) {
		if (priv->timeout_id != 0) {
			g_debug ("Cancelling outstanding arbitrary transitions.");
			g_source_remove (priv->timeout_id);
			priv->timeout_id = 0;
		}

		schedule_arbitrary_transition (self);
	}

	self->priv->dbus_activity_count = 0;
	g_object_notify (G_OBJECT (self), "dbus-activity-count");

	unfuzzed_transition_count = 0;
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

/**
 * dfsm_object_get_well_known_bus_names:
 * @self: a #DfsmObject
 *
 * Gets the value of the #DfsmObject:well-known-bus-names property.
 *
 * Return value: (transfer none): an array of the well-known bus names this object owns
 */
GPtrArray/*<string>*/ *
dfsm_object_get_well_known_bus_names (DfsmObject *self)
{
	g_return_val_if_fail (DFSM_IS_OBJECT (self), NULL);

	return self->priv->bus_names;
}

/**
 * dfsm_object_get_dbus_activity_count:
 * @self: a #DfsmObject
 *
 * Gets the value of the #DfsmObject:dbus-activity-count property.
 *
 * Return value: count of the number of D-Bus activities which have occurred in the simulation
 */
guint
dfsm_object_get_dbus_activity_count (DfsmObject *self)
{
	g_return_val_if_fail (DFSM_IS_OBJECT (self), 0);

	return self->priv->dbus_activity_count;
}
