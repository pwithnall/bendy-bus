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

#include "dfsm-ast-expression-function-call.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_expression_function_call_dispose (GObject *object);
static void dfsm_ast_expression_function_call_finalize (GObject *object);
static void dfsm_ast_expression_function_call_sanity_check (DfsmAstNode *node);
static void dfsm_ast_expression_function_call_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_expression_function_call_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static GVariantType *dfsm_ast_expression_function_call_calculate_type (DfsmAstExpression *self, DfsmEnvironment *environment);
static GVariant *dfsm_ast_expression_function_call_evaluate (DfsmAstExpression *self, DfsmEnvironment *environment, GError **error);
static gdouble dfsm_ast_expression_function_call_calculate_weight (DfsmAstExpression *self);

struct _DfsmAstExpressionFunctionCallPrivate {
	gchar *function_name;
	DfsmAstExpression *parameters;
};

G_DEFINE_TYPE (DfsmAstExpressionFunctionCall, dfsm_ast_expression_function_call, DFSM_TYPE_AST_EXPRESSION)

static void
dfsm_ast_expression_function_call_class_init (DfsmAstExpressionFunctionCallClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);
	DfsmAstExpressionClass *expression_class = DFSM_AST_EXPRESSION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstExpressionFunctionCallPrivate));

	gobject_class->dispose = dfsm_ast_expression_function_call_dispose;
	gobject_class->finalize = dfsm_ast_expression_function_call_finalize;

	node_class->sanity_check = dfsm_ast_expression_function_call_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_expression_function_call_pre_check_and_register;
	node_class->check = dfsm_ast_expression_function_call_check;

	expression_class->calculate_type = dfsm_ast_expression_function_call_calculate_type;
	expression_class->evaluate = dfsm_ast_expression_function_call_evaluate;
	expression_class->calculate_weight = dfsm_ast_expression_function_call_calculate_weight;
}

static void
dfsm_ast_expression_function_call_init (DfsmAstExpressionFunctionCall *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL, DfsmAstExpressionFunctionCallPrivate);
}

static void
dfsm_ast_expression_function_call_dispose (GObject *object)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (object)->priv;

	g_clear_object (&priv->parameters);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_expression_function_call_parent_class)->dispose (object);
}

static void
dfsm_ast_expression_function_call_finalize (GObject *object)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (object)->priv;

	g_free (priv->function_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_expression_function_call_parent_class)->finalize (object);
}

static void
dfsm_ast_expression_function_call_sanity_check (DfsmAstNode *node)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (node)->priv;

	g_assert (priv->function_name != NULL);

	g_assert (priv->parameters != NULL);
	dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->parameters));
}

static void
dfsm_ast_expression_function_call_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (node)->priv;

	if (dfsm_environment_function_exists (priv->function_name) == FALSE || dfsm_is_function_name (priv->function_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid function name: %s"), priv->function_name);
		return;
	}

	dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->parameters), environment, error);

	if (*error != NULL) {
		return;
	}
}

static void
dfsm_ast_expression_function_call_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (node)->priv;
	GVariantType *parameters_type, *return_type;

	dfsm_ast_node_check (DFSM_AST_NODE (priv->parameters), environment, error);

	if (*error != NULL) {
		return;
	}

	/* Check the types are compatible. */
	parameters_type = dfsm_ast_expression_calculate_type (priv->parameters, environment);
	return_type = dfsm_environment_function_calculate_type (priv->function_name, parameters_type, error);
	g_variant_type_free (parameters_type);

	if (*error != NULL) {
		return;
	}

	g_variant_type_free (return_type);
}

static GVariantType *
dfsm_ast_expression_function_call_calculate_type (DfsmAstExpression *expression, DfsmEnvironment *environment)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (expression)->priv;
	GVariantType *parameters_type, *return_type;

	parameters_type = dfsm_ast_expression_calculate_type (priv->parameters, environment);
	return_type = dfsm_environment_function_calculate_type (priv->function_name, parameters_type, NULL);
	g_assert (return_type != NULL);
	g_variant_type_free (parameters_type);

	return return_type;
}

static GVariant *
dfsm_ast_expression_function_call_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (expression)->priv;
	GVariant *parameters_value, *function_call_value;
	GError *child_error = NULL;

	/* Evaluate the parameters. */
	parameters_value = dfsm_ast_expression_evaluate (priv->parameters, environment, &child_error);

	if (child_error != NULL) {
		g_assert (parameters_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Delegate evaluation of the function to the function's evaluation function. Function function function. */
	function_call_value = dfsm_environment_function_evaluate (priv->function_name, parameters_value, environment, &child_error);
	g_variant_unref (parameters_value);

	if (child_error != NULL) {
		g_assert (function_call_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	return function_call_value;
}

static gdouble
dfsm_ast_expression_function_call_calculate_weight (DfsmAstExpression *self)
{
	return dfsm_ast_expression_calculate_weight (DFSM_AST_EXPRESSION_FUNCTION_CALL (self)->priv->parameters);
}

/**
 * dfsm_ast_expression_function_call_new:
 * @function_name: function name
 * @parameters: expression for function parameters
 *
 * Create a new #DfsmAstExpression representing a function call to @function_name with the given @parameters.
 *
 * Return value: (transfer full): a new expression
 */
DfsmAstExpression *
dfsm_ast_expression_function_call_new (const gchar *function_name, DfsmAstExpression *parameters)
{
	DfsmAstExpressionFunctionCall *function_call;
	DfsmAstExpressionFunctionCallPrivate *priv;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);
	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (parameters), NULL);

	function_call = g_object_new (DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL, NULL);
	priv = function_call->priv;

	priv->function_name = g_strdup (function_name);
	priv->parameters = g_object_ref (parameters);

	return DFSM_AST_EXPRESSION (function_call);
}
