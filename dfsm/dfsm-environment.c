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

#include "dfsm-environment.h"

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

struct _DfsmEnvironmentPrivate {
	GHashTable/*<string, VariableInfo>*/ *local_variables; /* string for variable name → variable */
	GHashTable/*<string, VariableInfo>*/ *object_variables; /* string for variable name → variable */
	/* TODO: Probably also want to store D-Bus interface XML here, plus function tables (i.e. deprecate dfsm-functions.[ch]) */
};

G_DEFINE_TYPE (DfsmEnvironment, dfsm_environment, G_TYPE_OBJECT)

static void
dfsm_environment_class_init (DfsmEnvironmentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmEnvironmentPrivate));

	gobject_class->dispose = dfsm_environment_dispose;
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

	g_hash_table_unref (priv->local_variables);
	g_hash_table_unref (priv->object_variables);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_environment_parent_class)->dispose (object);
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
	g_assert (create_if_nonexistent == TRUE || variable_info != NULL);

	/* Create the data if it doesn't exist. The members of variable_info will be filled in later by the caller. */
	if (variable_info == NULL) {
		variable_info = g_slice_new0 (VariableInfo);
		g_hash_table_insert (variable_map, g_strdup (variable_name), variable_info);
	}

	return variable_info;
}

/**
 * dfsm_environment_get_variable_type:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * Look up the type of the variable with the given @variable_name in @scope.
 *
 * Return value: (transfer full): type of the variable
 */
GVariantType *
dfsm_environment_get_variable_type (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	VariableInfo *variable_info;

	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);
	g_return_val_if_fail (variable_name != NULL, NULL);

	variable_info = look_up_variable_info (self, scope, variable_name, FALSE);

	return g_variant_type_copy (variable_info->type);
}

/**
 * dfsm_environment_get_variable_value:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * Look up the value of the variable with the given @variable_name in @scope.
 *
 * Return value: (transfer full): value of the variable
 */
GVariant *
dfsm_environment_get_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	VariableInfo *variable_info;

	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);
	g_return_val_if_fail (variable_name != NULL, NULL);

	variable_info = look_up_variable_info (self, scope, variable_name, FALSE);

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

/**
 * dfsm_environment_emit_signal:
 * @self: a #DfsmEnvironment
 * @signal_name: the name of the D-Bus signal to emit
 * @parameters: value of the parameters to the signal
 * @error: (allow-none): a #GError, or %NULL
 *
 * TODO
 */
void
dfsm_environment_emit_signal (DfsmEnvironment *self, const gchar *signal_name, GVariant *parameters, GError **error)
{
	g_return_if_fail (DFSM_IS_ENVIRONMENT (self));
	g_return_if_fail (signal_name != NULL);
	g_return_if_fail (parameters != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	/* TODO */
}
