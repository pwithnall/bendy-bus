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

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-environment.h"
#include "dfsm-marshal.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

typedef struct {
	GVariantType *type;
	GVariant *value;
} VariableInfo;

static VariableInfo *
variable_info_copy (VariableInfo *data)
{
	VariableInfo *new_data;

	new_data = g_slice_new (VariableInfo);

	new_data->type = g_variant_type_copy (data->type);
	new_data->value = g_variant_ref (data->value);

	return new_data;
}

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
	GHashTable/*<string, VariableInfo>*/ *local_variables, *local_variables_original; /* string for variable name → variable */
	GHashTable/*<string, VariableInfo>*/ *object_variables, *object_variables_original; /* string for variable name → variable */
	GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;
};

enum {
	PROP_INTERFACES = 1,
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
	 * DfsmEnvironment:interfaces:
	 *
	 * Information about the D-Bus interfaces in use by objects using this environment. An array of #GDBusInterfaceInfo structures.
	 */
	g_object_class_install_property (gobject_class, PROP_INTERFACES,
	                                 g_param_spec_boxed ("interfaces",
	                                                     "D-Bus Interface Information",
	                                                     "Information about the D-Bus interfaces in use by objects using this environment.",
	                                                     G_TYPE_PTR_ARRAY,
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
	self->priv->local_variables_original = NULL;
	self->priv->object_variables = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) variable_info_free);
	self->priv->object_variables_original = NULL;
}

static void
dfsm_environment_dispose (GObject *object)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	if (priv->interfaces != NULL) {
		g_ptr_array_unref (priv->interfaces);
		priv->interfaces = NULL;
	}

	if (priv->local_variables != NULL) {
		g_hash_table_unref (priv->local_variables);
		priv->local_variables = NULL;
	}

	if (priv->local_variables_original != NULL) {
		g_hash_table_unref (priv->local_variables_original);
		priv->local_variables_original = NULL;
	}

	if (priv->object_variables != NULL) {
		g_hash_table_unref (priv->object_variables);
		priv->object_variables = NULL;
	}

	if (priv->object_variables_original != NULL) {
		g_hash_table_unref (priv->object_variables_original);
		priv->object_variables_original = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_environment_parent_class)->dispose (object);
}

static void
dfsm_environment_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	switch (property_id) {
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
dfsm_environment_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	switch (property_id) {
		case PROP_INTERFACES:
			/* Construct-only */
			priv->interfaces = g_ptr_array_ref (g_value_get_boxed (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/*
 * dfsm_environment_new:
 * @interfaces: an array of #GDBusInterfaceInfo structures describing the interfaces used by objects using this environment
 *
 * Creates a new #DfsmEnvironment initialised with data from the given @interfaces, the default function table, and no local or object variables.
 *
 * Return value: (transfer full): a new #DfsmEnvironment
 */
DfsmEnvironment *
_dfsm_environment_new (GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces)
{
	g_return_val_if_fail (interfaces != NULL, NULL);
	g_return_val_if_fail (interfaces->len > 0, NULL);

	return g_object_new (DFSM_TYPE_ENVIRONMENT,
	                     "interfaces", interfaces,
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

static GHashTable *
copy_environment_hash_table (GHashTable *table)
{
	GHashTable *new_table;
	GHashTableIter iter;
	gchar *key;
	VariableInfo *value;

	new_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) variable_info_free);

	g_hash_table_iter_init (&iter, table);

	while (g_hash_table_iter_next (&iter, (gpointer*) &key, (gpointer*) &value) == TRUE) {
		g_hash_table_insert (new_table, g_strdup (key), variable_info_copy (value));
	}

	return new_table;
}

/**
 * dfsm_environment_save_reset_point:
 * @self: a #DfsmEnvironment
 *
 * Save the current values of all the variables in the environment into a special area of memory, where they'll stay untouched. If
 * dfsm_environment_reset() is called later, these original values will then replace the current values of variables in the environment. This is
 * a useful but hacky way of allowing the simulation to be reset.
 *
 * This must only be called once in the lifetime of a given #DfsmEnvironment.
 */
void
dfsm_environment_save_reset_point (DfsmEnvironment *self)
{
	DfsmEnvironmentPrivate *priv;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));

	priv = self->priv;

	g_assert (priv->local_variables_original == NULL && priv->object_variables_original == NULL);

	/* Copy local_variables into local_variables_original and the same for object_variables. */
	priv->local_variables_original = copy_environment_hash_table (priv->local_variables);
	priv->object_variables_original = copy_environment_hash_table (priv->object_variables);
}

/**
 * dfsm_environment_reset:
 * @self: a #DfsmEnvironment
 *
 * Reset the values of the variables in the environment to those saved previously using dfsm_environment_save_reset_point(). This function may only be
 * called after dfsm_environment_save_reset_point() has been called, but can then be called as many times as necessary.
 */
void
dfsm_environment_reset (DfsmEnvironment *self)
{
	DfsmEnvironmentPrivate *priv;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));

	priv = self->priv;

	g_assert (priv->local_variables_original != NULL && priv->object_variables_original != NULL);

	/* Copy local_variables_original over local_variables and the same for object_variables. */
	g_hash_table_unref (priv->local_variables);
	priv->local_variables = copy_environment_hash_table (priv->local_variables_original);

	g_hash_table_unref (priv->object_variables);
	priv->object_variables = copy_environment_hash_table (priv->object_variables_original);
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
	             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ but received type ‘%s’."),
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

static GVariant *
_keys_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariantBuilder builder;
	GVariant *child_variant;

	g_variant_builder_init (&builder, return_type);
	g_variant_iter_init (&iter, parameters);

	while ((child_variant = g_variant_iter_next_value (&iter)) != NULL) {
		/* child_variant is a dictionary entry of any type; we want its key. */
		GVariant *key;

		key = g_variant_get_child_value (child_variant, 0);
		g_variant_builder_add_value (&builder, key);
		g_variant_unref (key);

		g_variant_unref (child_variant);
	}

	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static GVariantType *
_pair_keys_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a?*)";
	const GVariantType *first_type;
	GVariantType *pair_type, *entry_type;

	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "pairKeys", parameters_supertype, parameters_type);
		return NULL;
	}

	/* For (a?*), return a{?*}. i.e. Return a dictionary mapping the elements of the first input array to the second element (which will
	 * typically be fuzzy and evaluated once per key). */
	first_type = g_variant_type_first (parameters_type);
	entry_type = g_variant_type_new_dict_entry (g_variant_type_element (first_type), g_variant_type_next (first_type));
	pair_type = g_variant_type_new_array (entry_type);
	g_variant_type_free (entry_type);

	return pair_type;
}

static GVariant *
_pair_keys_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantBuilder builder;
	GVariantIter iter;
	GVariant *keys, *value, *child_variant;

	keys = g_variant_get_child_value (parameters, 0);
	value = g_variant_get_child_value (parameters, 1);

	g_variant_builder_init (&builder, return_type);
	g_variant_iter_init (&iter, keys);

	while ((child_variant = g_variant_iter_next_value (&iter)) != NULL) {
		g_variant_builder_open (&builder, g_variant_type_element (return_type));
		g_variant_builder_add_value (&builder, child_variant);
		g_variant_builder_add_value (&builder, value);
		g_variant_builder_close (&builder);

		g_variant_unref (child_variant);
	}

	g_variant_unref (value);
	g_variant_unref (keys);

	return g_variant_ref_sink (g_variant_builder_end (&builder));
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
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the first item a subtype "
		               "of the element type of the second item, but received type ‘%s’."),
		             "inArray", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return a boolean. */
	return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
}

static GVariant *
_in_array_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariant *needle, *haystack, *child_variant;
	gboolean found = FALSE;

	needle = g_variant_get_child_value (parameters, 0);
	haystack = g_variant_get_child_value (parameters, 1);

	g_variant_iter_init (&iter, haystack);

	while (found == FALSE && (child_variant = g_variant_iter_next_value (&iter)) != NULL) {
		found = g_variant_equal (needle, child_variant);

		g_variant_unref (child_variant);
	}

	g_variant_unref (haystack);
	g_variant_unref (needle);

	/* Return found. */
	return g_variant_ref_sink (g_variant_new_boolean (found));
}

static GVariantType *
_array_get_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a*u*)";
	const GVariantType *array_type, *element_type;

	/* As well as conforming to the supertype, the two *s have to be in a subtype relationship. */
	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "arrayGet", parameters_supertype, parameters_type);
		return NULL;
	}

	array_type = g_variant_type_first (parameters_type);
	element_type = g_variant_type_next (g_variant_type_next (array_type));

	if (g_variant_type_is_subtype_of (element_type, g_variant_type_element (array_type)) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the third item a subtype "
		               "of the element type of the first item, but received type ‘%s’."),
		             "arrayGet", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return the element of the array. */
	return g_variant_type_copy (element_type);
}

static GVariant *
_array_get_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariant *haystack, *default_value, *child_variant;
	guint array_index;

	haystack = g_variant_get_child_value (parameters, 0);
	g_variant_get_child (parameters, 1, "u", &array_index);
	default_value = g_variant_get_child_value (parameters, 2);

	if (array_index >= g_variant_n_children (haystack)) {
		/* If the index is out of bounds, return the default value. */
		child_variant = g_variant_ref (default_value);
	} else {
		/* Get the child value. */
		child_variant = g_variant_get_child_value (haystack, array_index);
	}

	g_variant_unref (default_value);
	g_variant_unref (haystack);

	/* Return the child. */
	return child_variant;
}

static GVariantType *
_array_insert_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a*u*)";
	const GVariantType *array_type, *third_type;

	/* As well as conforming to the supertype, the two *s have to be in a subtype relationship. */
	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "arrayInsert", parameters_supertype, parameters_type);
		return NULL;
	}

	array_type = g_variant_type_first (parameters_type);
	third_type = g_variant_type_next (g_variant_type_next (array_type));

	if (g_variant_type_is_subtype_of (third_type, g_variant_type_element (array_type)) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the third item a subtype "
		               "of the element type of the first item, but received type ‘%s’."),
		             "arrayInsert", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return an array of the same type. */
	return g_variant_type_copy (array_type);
}

static GVariant *
_array_insert_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariantBuilder builder;
	GVariant *new_value, *old_array, *child_variant;
	guint array_index;

	old_array = g_variant_get_child_value (parameters, 0);
	g_variant_get_child (parameters, 1, "u", &array_index);
	new_value = g_variant_get_child_value (parameters, 2);

	/* Silently clamp the insertion index to the size of the input array. */
	array_index = MIN (array_index, g_variant_n_children (old_array));

	/* Copy the old array, inserting the new value. */
	g_variant_builder_init (&builder, return_type);
	g_variant_iter_init (&iter, old_array);

	while ((child_variant = g_variant_iter_next_value (&iter)) != NULL) {
		/* Insert the new value if we're at the right place.
		 * The array_index will then wrap around to G_MAXUINT so we won't consider it again. */
		if (array_index-- == 0) {
			g_variant_builder_add_value (&builder, new_value);
		}

		/* Insert an existing value. */
		g_variant_builder_add_value (&builder, child_variant);

		g_variant_unref (child_variant);
	}

	/* If the new value is being appended, append it. */
	if (array_index == 0) {
		g_variant_builder_add_value (&builder, new_value);
	}

	g_variant_unref (old_array);
	g_variant_unref (new_value);

	/* Return the new array. */
	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static GVariantType *
_array_remove_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a*u)";

	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "arrayRemove", parameters_supertype, parameters_type);
		return NULL;
	}

	/* Always return an array of the same type */
	return g_variant_type_copy (g_variant_type_first (parameters_type));
}

static GVariant *
_array_remove_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariantBuilder builder;
	GVariant *old_array, *child_variant;
	guint array_index;

	old_array = g_variant_get_child_value (parameters, 0);
	g_variant_get_child (parameters, 1, "u", &array_index);

	/* Silently clamp the removal index to the size of the input array. */
	array_index = MIN (array_index, g_variant_n_children (old_array) - 1);

	/* Copy the old array, removing the old value. */
	g_variant_builder_init (&builder, return_type);
	g_variant_iter_init (&iter, old_array);

	while ((child_variant = g_variant_iter_next_value (&iter)) != NULL) {
		/* Insert an existing value, unless it's the one to remove. */
		if (array_index-- != 0) {
			g_variant_builder_add_value (&builder, child_variant);
		}

		g_variant_unref (child_variant);
	}

	g_variant_unref (old_array);

	/* Return the new array. */
	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static GVariantType *
_dict_set_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a{?*}?*)";
	const GVariantType *dict_type, *key_type, *value_type;

	/* As well as conforming to the supertype, the two *s have to be in a subtype relationship, as do the two ?s. */
	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "dictSet", parameters_supertype, parameters_type);
		return NULL;
	}

	dict_type = g_variant_type_first (parameters_type);
	key_type = g_variant_type_next (dict_type);
	value_type = g_variant_type_next (key_type);

	if (g_variant_type_is_subtype_of (key_type, g_variant_type_key (g_variant_type_element (dict_type))) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the second item a subtype "
		               "of the key type of the first item, but received type ‘%s’."),
		             "dictSet", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	} else if (g_variant_type_is_subtype_of (value_type, g_variant_type_value (g_variant_type_element (dict_type))) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the third item a subtype "
		               "of the value type of the first item, but received type ‘%s’."),
		             "dictSet", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return a dict of the same type. */
	return g_variant_type_copy (dict_type);
}

static GVariant *
_dict_set_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariantBuilder builder;
	GVariant *new_key, *new_value, *old_dict, *child_entry;
	gboolean found = FALSE;

	old_dict = g_variant_get_child_value (parameters, 0);
	new_key = g_variant_get_child_value (parameters, 1);
	new_value = g_variant_get_child_value (parameters, 2);

	/* Copy the old dict, inserting the new key-value pair. */
	g_variant_builder_init (&builder, return_type);
	g_variant_iter_init (&iter, old_dict);

	while ((child_entry = g_variant_iter_next_value (&iter)) != NULL) {
		GVariant *current_key;

		/* If the current key matches the new key, replace the current value with the new value.
		 * Otherwise, just insert the current key-value pair. */
		current_key = g_variant_get_child_value (child_entry, 0);

		if (g_variant_compare (current_key, new_key) == 0) {
			g_assert (found == FALSE);
			found = TRUE;

			/* Replace existing pair. */
			g_variant_builder_open (&builder, g_variant_type_element (return_type));

			g_variant_builder_add_value (&builder, new_key);
			g_variant_builder_add_value (&builder, new_value);

			g_variant_builder_close (&builder);
		} else {
			/* Insert existing pair. */
			g_variant_builder_add_value (&builder, child_entry);
		}

		g_variant_unref (current_key);
		g_variant_unref (child_entry);
	}

	/* If the new pair wasn't already in the dict, add it to the end */
	if (found == FALSE) {
		g_variant_builder_open (&builder, g_variant_type_element (return_type));

		g_variant_builder_add_value (&builder, new_key);
		g_variant_builder_add_value (&builder, new_value);

		g_variant_builder_close (&builder);
	}

	g_variant_unref (new_value);
	g_variant_unref (new_key);
	g_variant_unref (old_dict);

	/* Return the new dict. */
	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static GVariantType *
_dict_unset_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a{?*}?)";
	const GVariantType *dict_type, *key_type;

	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "dictUnset", parameters_supertype, parameters_type);
		return NULL;
	}

	dict_type = g_variant_type_first (parameters_type);
	key_type = g_variant_type_next (dict_type);

	if (g_variant_type_is_subtype_of (key_type, g_variant_type_key (g_variant_type_element (dict_type))) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the second item a subtype "
		               "of the key type of the first item, but received type ‘%s’."),
		             "dictUnset", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return a dict of the same type */
	return g_variant_type_copy (dict_type);
}

static GVariant *
_dict_unset_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariantBuilder builder;
	GVariant *old_key, *old_dict, *child_entry;

	old_dict = g_variant_get_child_value (parameters, 0);
	old_key = g_variant_get_child_value (parameters, 1);

	/* Copy the old dict, removing the old key if it exists. */
	g_variant_builder_init (&builder, return_type);
	g_variant_iter_init (&iter, old_dict);

	while ((child_entry = g_variant_iter_next_value (&iter)) != NULL) {
		GVariant *current_key;

		/* Insert an existing value, unless it's the one to remove. */
		current_key = g_variant_get_child_value (child_entry, 0);

		if (g_variant_compare (current_key, old_key) != 0) {
			g_variant_builder_add_value (&builder, child_entry);
		}

		g_variant_unref (current_key);
		g_variant_unref (child_entry);
	}

	g_variant_unref (old_key);
	g_variant_unref (old_dict);

	/* Return the new dict. */
	return g_variant_ref_sink (g_variant_builder_end (&builder));
}

static GVariantType *
_dict_get_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(a{?*}?*)";
	const GVariantType *dict_type, *key_type, *value_type;

	/* As well as conforming to the supertype, the two *s have to be in a subtype relationship, as do the two ?s. */
	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "dictGet", parameters_supertype, parameters_type);
		return NULL;
	}

	dict_type = g_variant_type_first (parameters_type);
	key_type = g_variant_type_next (dict_type);
	value_type = g_variant_type_next (key_type);

	if (g_variant_type_is_subtype_of (key_type, g_variant_type_key (g_variant_type_element (dict_type))) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the second item a subtype "
		               "of the key type of the first item, but received type ‘%s’."),
		             "dictGet", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	} else if (g_variant_type_is_subtype_of (value_type, g_variant_type_value (g_variant_type_element (dict_type))) == FALSE) {
		gchar *parameters_supertype_string, *parameters_type_string;

		/* Error */
		parameters_supertype_string = g_variant_type_dup_string (parameters_supertype);
		parameters_type_string = g_variant_type_dup_string (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to function ‘%s’: expects type ‘%s’ with the third item a subtype "
		               "of the value type of the first item, but received type ‘%s’."),
		             "dictGet", parameters_supertype_string, parameters_type_string);

		g_free (parameters_type_string);
		g_free (parameters_supertype_string);

		return NULL;
	}

	/* Always return the value of the dict. */
	return g_variant_type_copy (value_type);
}

static GVariant *
_dict_get_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariantIter iter;
	GVariant *key, *dict, *default_value, *output_value = NULL, *child_entry;

	dict = g_variant_get_child_value (parameters, 0);
	key = g_variant_get_child_value (parameters, 1);
	default_value = g_variant_get_child_value (parameters, 2);

	/* Search through the dict for the given key. If it doesn't exist, return the default value (third parameter). */
	g_variant_iter_init (&iter, dict);

	while (output_value == NULL && (child_entry = g_variant_iter_next_value (&iter)) != NULL) {
		GVariant *current_key;

		current_key = g_variant_get_child_value (child_entry, 0);

		if (g_variant_compare (current_key, key) == 0) {
			output_value = g_variant_get_child_value (child_entry, 1);
		}

		g_variant_unref (current_key);
		g_variant_unref (child_entry);
	}

	/* Default value. */
	if (output_value == NULL) {
		output_value = g_variant_ref (default_value);
	}

	g_variant_unref (key);
	g_variant_unref (dict);
	g_variant_unref (default_value);

	/* Return the found or default value. */
	return output_value;
}

static GVariantType *
_struct_head_calculate_type (const GVariantType *parameters_type, GError **error)
{
	const GVariantType *parameters_supertype = (const GVariantType*) "(r)";

	/* Ideally, we'd have a structGet(ru) method which took the struct and a statically-defined integer giving the index. We could then check
	 * that the integer was a valid index into the struct, and calculate the type accordingly. However, we don't currently support statically
	 * defined values in the right way, so this isn't possible without introducing a runtime check on the index value. That's unsafe, so
	 * instead we use a head-and-tail approach, which can be safely typed without needing parametric typing. */
	if (g_variant_type_is_subtype_of (parameters_type, parameters_supertype) == FALSE) {
		/* Error */
		func_set_calculate_type_error (error, "structHead", parameters_supertype, parameters_type);
		return NULL;
	}

	/* Always return the value of the first element in the struct. */
	return g_variant_type_copy (g_variant_type_first (g_variant_type_first (parameters_type)));
}

static GVariant *
_struct_head_evaluate (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment)
{
	GVariant *_struct, *output_value;

	_struct = g_variant_get_child_value (parameters, 0);
	output_value = g_variant_get_child_value (_struct, 0);
	g_variant_unref (_struct);

	/* Return the output value. */
	return output_value;
}

typedef struct {
	const gchar *name;
	GVariantType *(*calculate_type_func) (const GVariantType *parameters_type, GError **error);
	/* Return value of evaluate_func must not be floating. */
	GVariant *(*evaluate_func) (GVariant *parameters, const GVariantType *return_type, DfsmEnvironment *environment);
} DfsmFunctionInfo;

static const DfsmFunctionInfo _function_info[] = {
	/* Name,		Calculate type func,		Evaluate func. */
	{ "keys",		_keys_calculate_type,		_keys_evaluate },
	{ "pairKeys",		_pair_keys_calculate_type,	_pair_keys_evaluate },
	{ "inArray",		_in_array_calculate_type,	_in_array_evaluate },
	{ "arrayGet",		_array_get_calculate_type,	_array_get_evaluate },
	{ "arrayInsert",	_array_insert_calculate_type,	_array_insert_evaluate },
	{ "arrayRemove",	_array_remove_calculate_type,	_array_remove_evaluate },
	{ "dictSet",		_dict_set_calculate_type,	_dict_set_evaluate },
	{ "dictUnset",		_dict_unset_calculate_type,	_dict_unset_evaluate },
	{ "dictGet",		_dict_get_calculate_type,	_dict_get_evaluate },
	{ "structHead",		_struct_head_calculate_type,	_struct_head_evaluate },
};

/*
 * dfsm_environment_get_function_info:
 * @function_name: name of the function to look up
 *
 * Look up static information about the given @function_name, such as its parameter and return types. If the function isn't known, %NULL will be
 * returned.
 *
 * Return value: (transfer none): information about the function, or %NULL
 */
static const DfsmFunctionInfo *
_get_function_info (const gchar *function_name)
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
 * dfsm_environment_function_exists:
 * @function_name: name of the function to check whether it exists
 *
 * Check whether the built-in function with name @function_name exists.
 *
 * Return value: %TRUE if the function exists, %FALSE otherwise
 */
gboolean
dfsm_environment_function_exists (const gchar *function_name)
{
	g_return_val_if_fail (function_name != NULL && *function_name != '\0', FALSE);

	return (_get_function_info (function_name) != NULL) ? TRUE : FALSE;
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

	function_info = _get_function_info (function_name);
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
 * dfsm_environment_function_evaluate:
 * @function_name: name of the function to evaluate
 * @parameters: the value of the input parameter (or parameters, if it's a tuple)
 * @environment: an environment containing all defined variables
 *
 * Evaluate @function_name when passed an input parameter, @parameters. The return value will have type as given by
 * dfsm_environment_function_calculate_type() for the type of @parameters.
 *
 * It is an error to pass an incompatible type in @parameters; or to pass a non-existent @function_name. The parameters must have previously been
 * type checked using dfsm_environment_function_calculate_type().
 *
 * Return value: (transfer full): return value of the function
 */
GVariant *
dfsm_environment_function_evaluate (const gchar *function_name, GVariant *parameters, DfsmEnvironment *environment)
{
	const DfsmFunctionInfo *function_info;
	GVariantType *return_type;
	GVariant *return_value = NULL;
	GError *child_error = NULL;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);
	g_return_val_if_fail (parameters != NULL, NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	function_info = _get_function_info (function_name);
	g_assert (function_info != NULL);
	g_assert (function_info->evaluate_func != NULL);

	/* Calculate the return type. This has the added side-effect of dynamically type checking the input parameters. */
	return_type = dfsm_environment_function_calculate_type (function_name, g_variant_get_type (parameters), &child_error);
	g_assert (child_error == NULL);

	/* Evaluate the function. */
	return_value = function_info->evaluate_func (parameters, return_type, environment);
	g_assert (return_value != NULL);

	g_variant_type_free (return_type);

	return return_value;
}

/**
 * dfsm_environment_emit_signal:
 * @self: a #DfsmEnvironment
 * @signal_name: the name of the D-Bus signal to emit
 * @parameters: value of the parameters to the signal
 *
 * Emit a signal indicating that the calling code intends for a D-Bus signal to be emitted with name @signal_name and parameters given by @parameters.
 * Note that this won't actually emit the signal on a bus instance; that's the responsibility of wrapper code listening to the
 * #DfsmEnvironment::signal-emission signal.
 */
void
dfsm_environment_emit_signal (DfsmEnvironment *self, const gchar *signal_name, GVariant *parameters)
{
	gchar *parameters_string;

	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (signal_name != NULL);
	g_return_if_fail (parameters != NULL);

	parameters_string = g_variant_print (parameters, FALSE);
	g_debug ("Emitting signal ‘%s’ in environment %p with parameters: %s", signal_name, self, parameters_string);
	g_free (parameters_string);

	/* Emit the signal. */
	g_signal_emit (self, environment_signals[SIGNAL_SIGNAL_EMISSION], g_quark_from_string (signal_name), signal_name, parameters);
}

/**
 * dfsm_environment_get_interfaces:
 * @self: a #DfsmEnvironment
 *
 * Gets the value of the #DfsmEnvironment:interfaces property.
 *
 * Return value: (transfer none): information about the D-Bus interfaces implemented by DFSMs which use this environment
 */
GPtrArray/*<GDBusInterfaceInfo>*/ *
dfsm_environment_get_interfaces (DfsmEnvironment *self)
{
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);

	return self->priv->interfaces;
}
