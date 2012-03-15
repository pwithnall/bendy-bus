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
 * SECTION:dfsm-ast-expression
 * @short_description: AST expression node
 * @stability: Unstable
 * @include: dfsm/dfsm-ast-expression.h
 *
 * Abstract AST node representing an evaluatable, typed expression with no side-effects.
 */

#include <glib.h>

#include "dfsm-ast-expression.h"
#include "dfsm-ast-object.h"

G_DEFINE_ABSTRACT_TYPE (DfsmAstExpression, dfsm_ast_expression, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_expression_class_init (DfsmAstExpressionClass *klass)
{
	/* Nothing to see here. */
}

static void
dfsm_ast_expression_init (DfsmAstExpression *self)
{
	/* Nothing to see here. */
}

/**
 * dfsm_ast_expression_calculate_type:
 * @self: a #DfsmAstExpression
 * @environment: a #DfsmEnvironment containing all defined variables
 *
 * Calculate the type of the given @expression. In some cases this may not be a definite type, for example if the expression is an empty data
 * structure. In most cases, however, the type will be definite.
 *
 * This assumes that the expression has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the expression
 */
GVariantType *
dfsm_ast_expression_calculate_type (DfsmAstExpression *self, DfsmEnvironment *environment)
{
	DfsmAstExpressionClass *klass;

	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	klass = DFSM_AST_EXPRESSION_GET_CLASS (self);

	g_assert (klass->calculate_type != NULL);
	return klass->calculate_type (self, environment);
}

/**
 * dfsm_ast_expression_evaluate:
 * @self: a #DfsmAstExpression
 * @environment: a #DfsmEnvironment containing all defined variables
 *
 * Evaluate the given @expression in the given @environment. This will not modify the environment.
 *
 * This assumes that the expression has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): non-floating value of the expression
 */
GVariant *
dfsm_ast_expression_evaluate (DfsmAstExpression *self, DfsmEnvironment *environment)
{
	DfsmAstExpressionClass *klass;
	GVariant *return_value;

	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	klass = DFSM_AST_EXPRESSION_GET_CLASS (self);

	g_assert (klass->evaluate != NULL);
	return_value = klass->evaluate (self, environment);

	g_assert (return_value != NULL && g_variant_is_floating (return_value) == FALSE);

	return return_value;
}

/**
 * dfsm_ast_expression_calculate_weight:
 * @self: a #DfsmAstExpression
 *
 * Recursively calculates the weight of the expression and its children for the purposes of fuzzing. The higher the expression's weight, the more
 * interesting it is to fuzz. For more information on weights, see dfsm_ast_data_structure_set_weight().
 *
 * Return value: weight of the expression
 */
gdouble
dfsm_ast_expression_calculate_weight (DfsmAstExpression *self)
{
	DfsmAstExpressionClass *klass;
	gdouble return_value;

	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (self), 0.0);

	klass = DFSM_AST_EXPRESSION_GET_CLASS (self);

	g_assert (klass->calculate_weight != NULL);
	return_value = klass->calculate_weight (self);

	/* Normalise the return value. */
	if (return_value < 0.0) {
		return_value = 0.0;
	}

	return return_value;
}
