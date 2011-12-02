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
	if (data->value != NULL) {
		g_variant_unref (data->value);
	}

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
	 * @parameters: the non-floating parameter (or structure of parameters) passed to the signal emission
	 *
	 * Emitted whenever a piece of code in a simulated DFSM emits a D-Bus signal. No code in the simulator or the environment will actually emit
	 * this D-Bus signal on a bus instance, but (for example) a wrapper which was listening to this signal could do so.
	 */
	environment_signals[SIGNAL_SIGNAL_EMISSION] = g_signal_new ("signal-emission",
	                                                            G_TYPE_FROM_CLASS (klass),
	                                                            G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
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

static GHashTable *
get_map_for_scope (DfsmEnvironment *self, DfsmVariableScope scope)
{
	/* Get the right map to extract the variable from. */
	switch (scope) {
		case DFSM_VARIABLE_SCOPE_LOCAL:
			return self->priv->local_variables;
		case DFSM_VARIABLE_SCOPE_OBJECT:
			return self->priv->object_variables;
		default:
			g_assert_not_reached ();
	}
}

static VariableInfo *
look_up_variable_info (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, gboolean create_if_nonexistent)
{
	GHashTable *variable_map;
	VariableInfo *variable_info;

	/* Grab the variable. */
	variable_map = get_map_for_scope (self, scope);
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
	g_assert (variable_info->type != NULL);

	return g_variant_type_copy (variable_info->type);
}

/**
 * dfsm_environment_set_variable_type:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 * @new_type: the new type for the variable
 *
 * Set the type of the variable named @variable_name in @scope to @new_value. The variable must not exist already, and must not have its value queried
 * by dfsm_environment_dup_variable_value() until it's set using dfsm_environment_set_variable_value().
 */
void
dfsm_environment_set_variable_type (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, const GVariantType *new_type)
{
	VariableInfo *variable_info;
	gchar *new_type_string;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (variable_name != NULL);
	g_return_if_fail (new_type != NULL);
	g_return_if_fail (g_variant_type_is_definite (new_type) == TRUE);

	new_type_string = g_variant_type_dup_string (new_type);
	g_debug ("Setting type of variable ‘%s’ (scope: %u) in environment %p to type: %s", variable_name, scope, self, new_type_string);
	g_free (new_type_string);

	variable_info = look_up_variable_info (self, scope, variable_name, TRUE);
	g_assert (variable_info != NULL);
	g_assert (variable_info->type == NULL);
	g_assert (variable_info->value == NULL);

	/* Set the new variable's type. */
	variable_info->type = g_variant_type_copy (new_type);
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
	g_assert (variable_info->value != NULL);

	return g_variant_ref (variable_info->value);
}

/**
 * dfsm_environment_set_variable_value:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 * @new_value: the new value for the variable
 *
 * Set the value of the variable named @variable_name in @scope to @new_value. The variable must have already been created using
 * dfsm_environment_set_variable_type() and the type of @new_value must match the type set then.
 */
void
dfsm_environment_set_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, GVariant *new_value)
{
	VariableInfo *variable_info;
	gchar *new_value_string;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (variable_name != NULL);
	g_return_if_fail (new_value != NULL);

	new_value_string = g_variant_print (new_value, FALSE);
	g_debug ("Setting variable ‘%s’ (scope: %u) in environment %p to value: %s", variable_name, scope, self, new_value_string);
	g_free (new_value_string);

	variable_info = look_up_variable_info (self, scope, variable_name, FALSE);
	g_assert (variable_info != NULL);
	g_assert (variable_info->type != NULL);
	g_assert (g_variant_type_is_subtype_of (g_variant_get_type (new_value), variable_info->type) == TRUE);

	/* Set the variable's value. Don't update its type. */
	g_variant_ref_sink (new_value);

	if (variable_info->value != NULL) {
		g_variant_unref (variable_info->value);
	}

	variable_info->value = new_value;
}

/**
 * dfsm_environment_unset_variable_value:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * Unset the value of the variable named @variable_name in @scope.
 */
void
dfsm_environment_unset_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	GHashTable *variable_map;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (variable_name != NULL);

	g_debug ("Unsetting variable ‘%s’ (scope: %u) in environment %p.", variable_name, scope, self);

	/* Remove the variable. */
	variable_map = get_map_for_scope (self, scope);
	g_hash_table_remove (variable_map, variable_name);
}

static void
func_set_calculate_type_error (GError **error, const gchar *function_name, const GVariantType *parameters_supertype,
                               const GVariantType *parameters_type)
{
	gchar *parameters_supertype_string, *parameters_type_string;

	/* Error */
	parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
	parameters_type_string = g_variant_type_dup_string (parameters_type);

	g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
	             "Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ but received type ‘%s’.",
	             function_name, parameters_supertype_string, parameters_type_string);

	g_free (parameters_type_string);
	g_free (parameters_supertype_string);
}

static GVariantType *
_keys_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "a{?*}";

	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "keys", parameters_supertype, parameters_type);
		return NULL;
	}

	/* For a{?*}, return a?. i.e. Return an array of the first element of the dictionary. */
	return g_variant_type_new_array (g_variant_type_first (g_variant_type_element (parameters_type)));
}

static GVariantType *
_pair_keys_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a?a*)";
	const GVariantType *first_type;
	GVariantType *pair_type, *entry_type;

	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "pairKeys", parameters_supertype, parameters_type);
		return NULL;
	}

	/* For (a?a*), return a{?*}. i.e. Return a dictionary mapping the elements of the first input array to the elements of the second. */
	first_type = g_variant_type_first (parameters_type);
	entry_type = g_variant_type_new_dict_entry (g_variant_type_element (first_type), g_variant_type_element (g_variant_type_next (first_type)));
	pair_type = g_variant_type_new_array (entry_type);
	g_variant_type_free (entry_type);

	return pair_type;
}

static GVariantType *
_in_array_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(*a*)";
	const GVariantType *first_type;

	/* As well as conforming to the supertype, the two *s have to be in a subtype relationship. */
	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "inArray", parameters_supertype, parameters_type);
		return NULL;
	}

	first_type = g_variant_type_first (parameters_type);

	if (g_variant_type_is_subtype_of (first_type, g_variant_type_element (g_variant_type_next (first_type))) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the first item a subtype "
		             "of the element type of the second item, but received type ‘%s’.",
		             "inArray", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return a boolean. */
	return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
}

static const DfsmFunctionInfo _function_info[] = {
	/* Name,	Calculate type func,		Evaluate func. */
	{ "keys",	_keys_calculate_type,		NULL /* TODO */ },
	{ "pairKeys",	_pair_keys_calculate_type,	NULL /* TODO */ },
	{ "inArray",	_in_array_calculate_type,	NULL /* TODO */ },
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
 * dfsm_environment_function_calculate_type:
 * @function_name: name of the function to calculate the return type for
 * @parameters_type: the type of the input parameter (or parameters, if it's a tuple)
 * @error: (allow-none): a #GError, or %NULL
 *
 * Calculate the return type of @function_name when passed an input parameter of type @parameters_type. If the input parameter type is incompatible
 * with the function, an error will be set and %NULL will be returned.
 *
 * It is an error to pass a non-definite type in @parameters_type; or to pass a non-existent @function_name.
 *
 * Return value: (transfer full): type of the return value of the function
 */
GVariantType *
dfsm_environment_function_calculate_type (const gchar *function_name, const GVariantType *parameters_type, GError **error)
{
	const DfsmFunctionInfo *function_info;
	GVariantType *return_type;
	GError *child_error = NULL;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);
	g_return_val_if_fail (parameters_type != NULL, NULL);
	g_return_val_if_fail (g_variant_type_is_definite (parameters_type) == TRUE, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	function_info = dfsm_environment_get_function_info (function_name);
	g_assert (function_info != NULL);

	g_assert (function_info->calculate_type_func != NULL);
	return_type = function_info->calculate_type_func (parameters_type, &child_error);
	g_assert ((return_type == NULL) != (child_error == NULL));

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
	}

	return return_type;
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
