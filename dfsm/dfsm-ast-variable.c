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

/**
 * SECTION:dfsm-ast-variable
 * @short_description: AST variable reference node
 * @stability: Unstable
 * @include: dfsm/dfsm-ast-variable.h
 *
 * AST node representing a variable reference, either for getting the value of the variable or for storing to the variable.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-ast-variable.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_variable_finalize (GObject *object);
static void dfsm_ast_variable_sanity_check (DfsmAstNode *node);
static void dfsm_ast_variable_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_variable_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstVariablePrivate {
	DfsmVariableScope scope;
	gchar *variable_name;
};

G_DEFINE_TYPE (DfsmAstVariable, dfsm_ast_variable, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_variable_class_init (DfsmAstVariableClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstVariablePrivate));

	gobject_class->finalize = dfsm_ast_variable_finalize;

	node_class->sanity_check = dfsm_ast_variable_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_variable_pre_check_and_register;
	node_class->check = dfsm_ast_variable_check;
}

static void
dfsm_ast_variable_init (DfsmAstVariable *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_VARIABLE, DfsmAstVariablePrivate);
}

static void
dfsm_ast_variable_finalize (GObject *object)
{
	DfsmAstVariablePrivate *priv = DFSM_AST_VARIABLE (object)->priv;

	g_free (priv->variable_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_variable_parent_class)->finalize (object);
}

static void
dfsm_ast_variable_sanity_check (DfsmAstNode *node)
{
	DfsmAstVariablePrivate *priv = DFSM_AST_VARIABLE (node)->priv;

	g_assert (priv->variable_name != NULL);

	switch (priv->scope) {
		case DFSM_VARIABLE_SCOPE_LOCAL:
		case DFSM_VARIABLE_SCOPE_OBJECT:
			/* Nothing to do */
			break;
		default:
			g_assert_not_reached ();
	}
}

static void
dfsm_ast_variable_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstVariablePrivate *priv = DFSM_AST_VARIABLE (node)->priv;

	if (dfsm_is_variable_name (priv->variable_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid variable name: %s"), priv->variable_name);
		return;
	}
}

static void
dfsm_ast_variable_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstVariablePrivate *priv = DFSM_AST_VARIABLE (node)->priv;

	/* Check the variable exists in this scope. */
	if (dfsm_environment_has_variable (environment, priv->scope, priv->variable_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Undeclared variable referenced: %s"), priv->variable_name);
		return;
	}
}

/**
 * dfsm_ast_variable_new:
 * @scope: scope of the variable reference
 * @variable_name: name of the variable being referenced
 *
 * Create a new #DfsmAstVariable representing a variable dereferencing operation.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstVariable *
dfsm_ast_variable_new (DfsmVariableScope scope, const gchar *variable_name)
{
	DfsmAstVariable *variable;
	DfsmAstVariablePrivate *priv;

	g_return_val_if_fail (variable_name != NULL && *variable_name != '\0', NULL);

	variable = g_object_new (DFSM_TYPE_AST_VARIABLE, NULL);
	priv = variable->priv;

	priv->scope = scope;
	priv->variable_name = g_strdup (variable_name);

	return variable;
}

/**
 * dfsm_ast_variable_calculate_type:
 * @self: a #DfsmAstVariable
 * @environment: a #DfsmEnvironment containing all variables
 *
 * Calculate the type of the given variable.
 *
 * This assumes that the variable has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the variable
 */
GVariantType *
dfsm_ast_variable_calculate_type (DfsmAstVariable *self, DfsmEnvironment *environment)
{
	g_return_val_if_fail (DFSM_IS_AST_VARIABLE (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	return dfsm_environment_dup_variable_type (environment, self->priv->scope, self->priv->variable_name);
}

/**
 * dfsm_ast_variable_to_variant:
 * @self: a #DfsmAstVariable
 * @environment: a #DfsmEnvironment containing all variables
 *
 * Convert the value stored by @self to a #GVariant in the given @environment.
 *
 * This assumes that the variable has been successfully checked by dfsm_ast_node_check() beforehand. It is an error to call this function otherwise.
 *
 * Return value: (transfer full): the #GVariant version of the variable's value
 */
GVariant *
dfsm_ast_variable_to_variant (DfsmAstVariable *self, DfsmEnvironment *environment)
{
	GVariant *return_value;

	g_return_val_if_fail (DFSM_IS_AST_VARIABLE (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	return_value = dfsm_environment_dup_variable_value (environment, self->priv->scope, self->priv->variable_name);
	g_assert (return_value != NULL && g_variant_is_floating (return_value) == FALSE);

	return return_value;
}

/**
 * dfsm_ast_variable_set_from_variant:
 * @self: a #DfsmAstVariable
 * @environment: a #DfsmEnvironment containing all variables
 * @new_value: the new value to assign to the variable
 *
 * Set the given variable's value in @environment to the #GVariant value given in @new_value. This will overwrite any existing value stored for
 * the given variable, and won't recursively assign to structures inside the variable's existing value.
 */
void
dfsm_ast_variable_set_from_variant (DfsmAstVariable *self, DfsmEnvironment *environment, GVariant *new_value)
{
	g_return_if_fail (DFSM_IS_AST_VARIABLE (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (new_value != NULL);

	dfsm_environment_set_variable_value (environment, self->priv->scope, self->priv->variable_name, new_value);
}
