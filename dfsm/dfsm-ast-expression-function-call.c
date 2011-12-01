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

#include "dfsm-ast-expression-function-call.h"
#include "dfsm-parser.h"

static void dfsm_ast_expression_function_call_dispose (GObject *object);
static void dfsm_ast_expression_function_call_finalize (GObject *object);
static void dfsm_ast_expression_function_call_sanity_check (DfsmAstNode *node);
static void dfsm_ast_expression_function_call_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_expression_function_call_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static GVariantType *dfsm_ast_expression_function_call_calculate_type (DfsmAstExpression *self, DfsmEnvironment *environment);
static GVariant *dfsm_ast_expression_function_call_evaluate (DfsmAstExpression *self, DfsmEnvironment *environment, GError **error);

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
}

static void
dfsm_ast_expression_function_call_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (node)->priv;
	const DfsmFunctionInfo *function_info;

	function_info = dfsm_environment_get_function_info (priv->function_name);

	if (function_info == NULL || dfsm_is_function_name (priv->function_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid function name: %s", priv->function_name);
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
	const DfsmFunctionInfo *function_info;
	GVariantType *parameters_type;

	dfsm_ast_node_check (DFSM_AST_NODE (priv->parameters), environment, error);

	if (*error != NULL) {
		return;
	}

	function_info = dfsm_environment_get_function_info (priv->function_name);
	g_assert (function_info != NULL);

	parameters_type = dfsm_ast_expression_calculate_type (priv->parameters, environment);

	if (g_variant_type_is_subtype_of (parameters_type, function_info->parameters_type) == FALSE) {
		gchar *formal, *actual;

		formal = g_variant_type_dup_string (function_info->parameters_type);
		actual = g_variant_type_dup_string (parameters_type);

		g_variant_type_free (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch between formal and actual parameters to function ‘%s’: expects type %s but received type %s.",
		             priv->function_name, formal, actual);
		return;
	}

	g_variant_type_free (parameters_type);
}

static GVariantType *
dfsm_ast_expression_function_call_calculate_type (DfsmAstExpression *expression, DfsmEnvironment *environment)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (expression)->priv;
	const DfsmFunctionInfo *function_info;

	/* Type of the function call is just the return type of the function. */
	function_info = dfsm_environment_get_function_info (priv->function_name);
	g_assert (function_info != NULL);

	return g_variant_type_copy (function_info->return_type);
}

static GVariant *
dfsm_ast_expression_function_call_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionFunctionCallPrivate *priv = DFSM_AST_EXPRESSION_FUNCTION_CALL (expression)->priv;
	const DfsmFunctionInfo *function_info;
	GVariant *parameters_value, *function_call_value;
	GError *child_error = NULL;

	/* Look up the function information. */
	function_info = dfsm_environment_get_function_info (priv->function_name);
	g_assert (function_info != NULL);

	/* Evaluate the parameters. */
	parameters_value = dfsm_ast_expression_evaluate (priv->parameters, environment, &child_error);

	if (child_error != NULL) {
		g_assert (parameters_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Delegate evaluation of the function to the function's evaluation function. Function function function. */
	g_assert (function_info->evaluate_func != NULL);
	function_call_value = function_info->evaluate_func (parameters_value, environment, error);
	g_variant_unref (parameters_value);

	return function_call_value;
}

/**
 * dfsm_ast_expression_function_call_new:
 * @function_name: function name
 * @parameters: expression for function parameters
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstExpression representing a function call to @function_name with the given @parameters.
 *
 * Return value: (transfer full): a new expression
 */
DfsmAstExpression *
dfsm_ast_expression_function_call_new (const gchar *function_name, DfsmAstExpression *parameters, GError **error)
{
	DfsmAstExpressionFunctionCall *function_call;
	DfsmAstExpressionFunctionCallPrivate *priv;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);
	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (parameters), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	function_call = g_object_new (DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL, NULL);
	priv = function_call->priv;

	priv->function_name = g_strdup (function_name);
	priv->parameters = g_object_ref (parameters);

	return DFSM_AST_EXPRESSION (function_call);
}
