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

static void dfsm_environment_dispose (GObject *object);

struct _DfsmEnvironmentPrivate {
	GHashTable/*<string, DfsmAstDataItem>*/ *variables; /* string for variable name → data item */
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
}

static void
dfsm_environment_dispose (GObject *object)
{
	DfsmEnvironmentPrivate *priv = DFSM_ENVIRONMENT (object)->priv;

	g_hash_table_unref (priv->variables);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_environment_parent_class)->dispose (object);
}

/**
 * dfsm_environment_get_variable_type:
 * @self: a #DfsmEnvironment
 * @scope: the scope of the variable
 * @variable_name: the name of the variable in the given @scope
 *
 * TODO
 *
 * Return value: (transfer full): TODO
 */
GVariantType *
dfsm_environment_get_variable_type (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (self), NULL);
	g_return_val_if_fail (variable_name != NULL, NULL);

	/* TODO */

	return NULL;
}

/* TODO */
GVariant *
dfsm_environment_get_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name)
{
	/* TODO */

	return NULL;
}

/* TODO */
void
dfsm_environment_set_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, GVariant *new_value)
{
	/* TODO */
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
