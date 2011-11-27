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

#include <string.h>
#include <glib.h>

#include "dfsm-environment.h"
#include "dfsm-marshal.h"
#include "dfsm-parser.h"

typedef struct {
	GVariantType *type;
	GVariant *value;
} VariableInfo;

static void
variable_info_free (VariableInfo *data)
{
	g_variant_unref (data->value);
	g_variant_type_free (data->type);
	g_slice_free (VariableInfo, data);
}

static void dfsm_environment_dispose (GObject *object);
static void dfsm_environment_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dfsm_environment_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _DfsmEnvironmentPrivate {
	GHashTable/*<string, VariableInfo>*/ *local_variables; /* string for variable name → variable */
	GHashTable/*<string, VariableInfo>*/ *object_variables; /* string for variable name → variable */
	GDBusNodeInfo *dbus_node_info;
};

enum {
	PROP_DBUS_NODE_INFO = 1,
};

enum {
	SIGNAL_SIGNAL_EMISSION,
	LAST_SIGNAL,
};

static guint environment_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (DfsmEnvironment, dfsm_environment, G_TYPE_OBJECT)

static void
dfsm_environment_class_init (DfsmEnvironmentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmEnvironmentPrivate));

	gobject_class->get_property = dfsm_environment_get_property;
	gobject_class->set_property = dfsm_environment_set_property;
	gobject_class->dispose = dfsm_environment_dispose;

	/**
	 * DfsmEnvironment:dbus-node-info:
	 *
	 * Information about the D-Bus interfaces in use by objects using this environment.
	 */
	g_object_class_install_property (gobject_class, PROP_DBUS_NODE_INFO,
	                                 g_param_spec_boxed ("dbus-node-info",
	                                                     "D-Bus Node Information",
	                                                     "Information about the D-Bus interfaces in use by objects using this environment.",
	                                                     G_TYPE_DBUS_NODE_INFO,
	                                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DfsmEnvironment::signal-emission:
	 * @parameters: the parameter (or structure of parameters) passed to the signal emission
	 *
	 * Emitted whenever a piece of code in a simulated DFSM emits a D-Bus signal. No code in the simulator or the environment will actually emit
	 * this D-Bus signal on a bus instance, but (for example) a wrapper which was listening to this signal could do so.
	 */
	environment_signals[SIGNAL_SIGNAL_EMISSION] = g_signal_new ("signal-emission",
	                                                            G_TYPE_FROM_CLASS (klass),
	                                                            G_SIGNAL_RUN_LAST,
	                                                            0, NULL, NULL,
	                                                            dfsm_marshal_VOID__STRING_VARIANT,
	                                                            G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VARIANT);
}

static void
dfsm_environment_init (DfsmEnvironment *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_ENVIRONMENT, DfsmEnvironmentPrivate);

	self->priv->local_variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) variable_info_free);
	self->priv->object_variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) variable_info_free);
}

static void
dfsm_environment_dispose (GObject *object)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	if (priv->dbus_node_info != NULL) {
		g_dbus_node_info_unref (priv->dbus_node_info);
		priv->dbus_node_info = NULL;
	}

	if (priv->local_variables != NULL) {
		g_hash_table_unref (priv->local_variables);
		priv->local_variables = NULL;
	}

	if (priv->object_variables != NULL) {
		g_hash_table_unref (priv->object_variables);
		priv->object_variables = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_environment_parent_class)->dispose (object);
}

static void
dfsm_environment_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	switch (property_id) {
		case PROP_DBUS_NODE_INFO:
			g_value_set_boxed (value, priv->dbus_node_info);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dfsm_environment_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	switch (property_id) {
		case PROP_DBUS_NODE_INFO:
			/* Construct-only */
			priv->dbus_node_info = g_dbus_node_info_ref (g_value_get_boxed (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/*
 * dfsm_environment_new:
 * @dbus_node_info: a #GDBusNodeInfo structure describing the interfaces used by objects using this environment
 *
 * Creates a new #DfsmEnvironment initialised with data from the given @dbus_node_info, the default function table, and no local or object variables.
 *
 * Return value: (transfer full): a new #DfsmEnvironment
 */
DfsmEnvironment *
_dfsm_environment_new (GDBusNodeInfo *dbus_node_info)
{
	g_return_val_if_fail (dbus_node_info != NULL, NULL);

	return g_object_new (DFSM_TYPE_ENVIRONMENT,
	                     "dbus-node-info", dbus_node_info,
	                     NULL);
}

static VariableInfo *
look_up_variable_info (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, gboolean create_if_nonexistent)
{
	GHashTable *variable_map;
	VariableInfo *variable_info;

	/* Get the right map to extract the variable from. */
	switch (scope) {
		case DFSM_VARIABLE_SCOPE_LOCAL:
			variable_map = self->priv->local_variables;
			break;
		case DFSM_VARIABLE_SCOPE_OBJECT:
			variable_map = self->priv->object_variables;
			break;
		default:
			g_assert_not_reached ();
	}

	/* Grab the variable. */
	variable_info = g_hash_table_lookup (variable_map, variable_name);

	/* Create the data if it doesn't exist. The members of variable_info will be filled in later by the caller. */
	if (create_if_nonexistent == TRUE && variable_info == NULL) {
		variable_info = g_slice_new0 (VariableInfo);
		g_hash_table_insert (variable_map, g_strdup (variable_name), variable_info);
	}

	return variable_info;
}

/**
 * dfsm_environment_has_variable:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * Look up the value of the variable with the given @variable_name in @scope and see if it exists. This should only be used during the construction
 * of abstract syntax trees, since all variables should be guaranteed to exist afterwards.
 *
 * Return value: %TRUE if the variable exists in the environment, %FALSE otherwise
 */
gboolean
dfsm_environment_has_variable (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	VariableInfo *variable_info;

	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), FALSE);
	g_return_val_if_fail (variable_name != NULL, FALSE);

	variable_info = look_up_variable_info (self, scope, variable_name, FALSE);

	return (variable_info != NULL) ? TRUE : FALSE;
}

/**
 * dfsm_environment_dup_variable_type:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * Look up the type of the variable with the given @variable_name in @scope.
 *
 * Return value: (transfer full): type of the variable
 */
GVariantType *
dfsm_environment_dup_variable_type (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	VariableInfo *variable_info;

	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);
	g_return_val_if_fail (variable_name != NULL, NULL);

	variable_info = look_up_variable_info (self, scope, variable_name, FALSE);
	g_assert (variable_info != NULL);

	return g_variant_type_copy (variable_info->type);
}

/**
 * dfsm_environment_dup_variable_value:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * Look up the value of the variable with the given @variable_name in @scope.
 *
 * Return value: (transfer full): value of the variable
 */
GVariant *
dfsm_environment_dup_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	VariableInfo *variable_info;

	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);
	g_return_val_if_fail (variable_name != NULL, NULL);

	variable_info = look_up_variable_info (self, scope, variable_name, FALSE);
	g_assert (variable_info != NULL);

	return g_variant_ref (variable_info->value);
}

/**
 * dfsm_environment_set_variable_value:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 * @new_value: the new value for the variable
 *
 * Set the value of the variable named @variable_name in @scope to @new_value. If the variable doesn't exist already, it is created.
 */
void
dfsm_environment_set_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, GVariant *new_value)
{
	VariableInfo *variable_info;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (variable_name != NULL);

	variable_info = look_up_variable_info (self, scope, variable_name, TRUE);
	g_assert (variable_info != NULL);

	if (variable_info->value != NULL) {
		/* Variable already exists */
		g_variant_unref (variable_info->value);
		variable_info->value = g_variant_ref (new_value);
	} else {
		/* New variable */
		variable_info->type = g_variant_type_copy (g_variant_get_type (new_value));
		variable_info->value = g_variant_ref (new_value);
	}
}

static const DfsmFunctionInfo _function_info[] = {
	/* TODO: Not typesafe, but I'm not sure we want polymorphism */
	/* Name,	Parameters type,		Return type,			Evaluate func. */
	{ "keys",	G_VARIANT_TYPE_ARRAY,		G_VARIANT_TYPE_ANY,		NULL /* TODO */ },
	{ "newObject",	G_VARIANT_TYPE_OBJECT_PATH,	(const GVariantType*) "(os)",	NULL /* TODO */ },
	{ "pairKeys",	(const GVariantType*) "(a?a*)",	(const GVariantType*) "a{?*}",	NULL /* TODO */ },
};

/**
 * dfsm_environment_get_function_info:
 * @function_name: name of the function to look up
 *
 * Look up static information about the given @function_name, such as its parameter and return types. If the function isn't known, %NULL will be
 * returned.
 *
 * Return value: (transfer none): information about the function, or %NULL
 */
const DfsmFunctionInfo *
dfsm_environment_get_function_info (const gchar *function_name)
{
	guint i;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);

	/* Do a linear search for now, since there aren't many functions at all. */
	for (i = 0; i < G_N_ELEMENTS (_function_info); i++) {
		if (strcmp (function_name, _function_info[i].name) == 0) {
			return &_function_info[i];
		}
	}

	return NULL;
}

/**
 * dfsm_environment_emit_signal:
 * @self: a #DfsmEnvironment
 * @signal_name: the name of the D-Bus signal to emit
 * @parameters: value of the parameters to the signal
 * @error: (allow-none): a #GError, or %NULL
 *
 * Emit a signal indicating that the calling code intends for a D-Bus signal to be emitted with name @signal_name and parameters given by @parameters.
 * Note that this won't actually emit the signal on a bus instance; that's the responsibility of wrapper code listening to the
 * #DfsmEnvironment::signal-emission signal.
 */
void
dfsm_environment_emit_signal (DfsmEnvironment *self, const gchar *signal_name, GVariant *parameters, GError **error)
{
	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (signal_name != NULL);
	g_return_if_fail (parameters != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	/* Emit the signal. */
	g_signal_emit (self, environment_signals[SIGNAL_SIGNAL_EMISSION], g_quark_from_string (signal_name), signal_name, parameters);
}

/**
 * dfsm_environment_get_dbus_node_info:
 * @self: a #DfsmEnvironment
 *
 * Gets the value of the #DfsmEnvironment:dbus-node-info property.
 *
 * Return value: (transfer none): information about the D-Bus interfaces implemented by DFSMs which use this environment
 */
GDBusNodeInfo *
dfsm_environment_get_dbus_node_info (DfsmEnvironment *self)
{
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);

	return self->priv->dbus_node_info;
}
