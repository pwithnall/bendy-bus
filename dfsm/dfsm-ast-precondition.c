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

#include "dfsm-ast-precondition.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_precondition_dispose (GObject *object);
static void dfsm_ast_precondition_finalize (GObject *object);
static void dfsm_ast_precondition_sanity_check (DfsmAstNode *node);
static void dfsm_ast_precondition_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_precondition_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstPreconditionPrivate {
	gchar *error_name; /* nullable */
	DfsmAstExpression *condition;
};

G_DEFINE_TYPE (DfsmAstPrecondition, dfsm_ast_precondition, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_precondition_class_init (DfsmAstPreconditionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstPreconditionPrivate));

	gobject_class->dispose = dfsm_ast_precondition_dispose;
	gobject_class->finalize = dfsm_ast_precondition_finalize;

	node_class->sanity_check = dfsm_ast_precondition_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_precondition_pre_check_and_register;
	node_class->check = dfsm_ast_precondition_check;
}

static void
dfsm_ast_precondition_init (DfsmAstPrecondition *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_PRECONDITION, DfsmAstPreconditionPrivate);
}

static void
dfsm_ast_precondition_dispose (GObject *object)
{
	DfsmAstPreconditionPrivate *priv = DFSM_AST_PRECONDITION (object)->priv;

	g_clear_object (&priv->condition);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_precondition_parent_class)->dispose (object);
}

static void
dfsm_ast_precondition_finalize (GObject *object)
{
	DfsmAstPreconditionPrivate *priv = DFSM_AST_PRECONDITION (object)->priv;

	g_free (priv->error_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_precondition_parent_class)->finalize (object);
}

static void
dfsm_ast_precondition_sanity_check (DfsmAstNode *node)
{
	DfsmAstPreconditionPrivate *priv = DFSM_AST_PRECONDITION (node)->priv;

	g_assert (priv->condition != NULL);
	dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->condition));
}

static void
dfsm_ast_precondition_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstPreconditionPrivate *priv = DFSM_AST_PRECONDITION (node)->priv;

	if (priv->error_name != NULL && g_dbus_is_member_name (priv->error_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus error name: %s"), priv->error_name);
		return;
	}

	dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->condition), environment, error);

	if (*error != NULL) {
		return;
	}
}

static void
dfsm_ast_precondition_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstPreconditionPrivate *priv = DFSM_AST_PRECONDITION (node)->priv;
	GVariantType *condition_type;

	dfsm_ast_node_check (DFSM_AST_NODE (priv->condition), environment, error);

	if (*error != NULL) {
		return;
	}

	/* Check that the precondition is a boolean */
	condition_type = dfsm_ast_expression_calculate_type (priv->condition, environment);

	if (g_variant_type_equal (condition_type, G_VARIANT_TYPE_BOOLEAN) == FALSE) {
		gchar *condition_type_string;

		condition_type_string = g_variant_type_dup_string (condition_type);

		g_variant_type_free (condition_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Incorrect type for precondition expression: expects type ‘%s’ but received type ‘%s’."),
		             "b", condition_type_string);

		g_free (condition_type_string);

		return;
	}

	g_variant_type_free (condition_type);
}

/**
 * dfsm_ast_precondition_new:
 * @error_name: (allow-none): name of the D-Bus error to throw on precondition failure, or %NULL
 * @condition: the condition to fulfil for the precondition
 *
 * Create a new #DfsmAstPrecondition for the given @condition.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstPrecondition *
dfsm_ast_precondition_new (const gchar *error_name /* nullable */, DfsmAstExpression *condition)
{
	DfsmAstPrecondition *precondition;
	DfsmAstPreconditionPrivate *priv;

	g_return_val_if_fail (error_name == NULL || *error_name != '\0', NULL);
	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (condition), NULL);

	precondition = g_object_new (DFSM_TYPE_AST_PRECONDITION, NULL);
	priv = precondition->priv;

	priv->error_name = g_strdup (error_name);
	priv->condition = g_object_ref (condition);

	return precondition;
}

/**
 * dfsm_ast_precondition_check_is_satisfied:
 * @self: a #DfsmAstPrecondition
 * @environment: a #DfsmEnvironment containing all variables
 * @error: a #GError
 *
 * Check whether the precondition is satisfied by the given @environment. This will evaluate the precondition's statement in the given @environment,
 * and return the boolean value of the statement.
 *
 * If an error is encountered while evaluating the statement, @error will be set and %FALSE will be returned.
 *
 * Return value: %TRUE if the precondition is satisfied by @environment, %FALSE otherwise
 */
gboolean
dfsm_ast_precondition_check_is_satisfied (DfsmAstPrecondition *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstPreconditionPrivate *priv;
	GVariant *condition_value;
	gboolean condition_holds;
	GError *child_error = NULL;

	g_return_val_if_fail (DFSM_IS_AST_PRECONDITION (self), FALSE);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), FALSE);
	g_return_val_if_fail (error != NULL && *error == NULL, FALSE);

	priv = self->priv;

	/* Evaluate the condition. */
	condition_value = dfsm_ast_expression_evaluate (priv->condition, environment, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return FALSE;
	}

	/* If the condition doesn't hold and we have an error, throw that. */
	condition_holds = g_variant_get_boolean (condition_value);

	if (condition_holds == FALSE && priv->error_name != NULL) {
		g_dbus_error_set_dbus_error (error, priv->error_name, _("Precondition failed."), NULL);
	}

	g_variant_unref (condition_value);

	return condition_holds;
}
