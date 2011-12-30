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

#include "dfsm-ast-expression-binary.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_expression_binary_dispose (GObject *object);
static void dfsm_ast_expression_binary_sanity_check (DfsmAstNode *node);
static void dfsm_ast_expression_binary_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_expression_binary_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static GVariantType *dfsm_ast_expression_binary_calculate_type (DfsmAstExpression *self, DfsmEnvironment *environment);
static GVariant *dfsm_ast_expression_binary_evaluate (DfsmAstExpression *self, DfsmEnvironment *environment, GError **error);
static gdouble dfsm_ast_expression_binary_calculate_weight (DfsmAstExpression *self);

struct _DfsmAstExpressionBinaryPrivate {
	DfsmAstExpressionBinaryType expression_type;
	DfsmAstExpression *left_node;
	DfsmAstExpression *right_node;
};

G_DEFINE_TYPE (DfsmAstExpressionBinary, dfsm_ast_expression_binary, DFSM_TYPE_AST_EXPRESSION)

static void
dfsm_ast_expression_binary_class_init (DfsmAstExpressionBinaryClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);
	DfsmAstExpressionClass *expression_class = DFSM_AST_EXPRESSION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstExpressionBinaryPrivate));

	gobject_class->dispose = dfsm_ast_expression_binary_dispose;

	node_class->sanity_check = dfsm_ast_expression_binary_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_expression_binary_pre_check_and_register;
	node_class->check = dfsm_ast_expression_binary_check;

	expression_class->calculate_type = dfsm_ast_expression_binary_calculate_type;
	expression_class->evaluate = dfsm_ast_expression_binary_evaluate;
	expression_class->calculate_weight = dfsm_ast_expression_binary_calculate_weight;
}

static void
dfsm_ast_expression_binary_init (DfsmAstExpressionBinary *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_EXPRESSION_BINARY, DfsmAstExpressionBinaryPrivate);
}

static void
dfsm_ast_expression_binary_dispose (GObject *object)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (object)->priv;

	g_clear_object (&priv->right_node);
	g_clear_object (&priv->left_node);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_expression_binary_parent_class)->dispose (object);
}

static void
dfsm_ast_expression_binary_sanity_check (DfsmAstNode *node)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (node)->priv;

	switch (priv->expression_type) {
		case DFSM_AST_EXPRESSION_BINARY_TIMES:
		case DFSM_AST_EXPRESSION_BINARY_DIVIDE:
		case DFSM_AST_EXPRESSION_BINARY_MODULUS:
		case DFSM_AST_EXPRESSION_BINARY_PLUS:
		case DFSM_AST_EXPRESSION_BINARY_MINUS:
		case DFSM_AST_EXPRESSION_BINARY_LT:
		case DFSM_AST_EXPRESSION_BINARY_LTE:
		case DFSM_AST_EXPRESSION_BINARY_GT:
		case DFSM_AST_EXPRESSION_BINARY_GTE:
		case DFSM_AST_EXPRESSION_BINARY_EQ:
		case DFSM_AST_EXPRESSION_BINARY_NEQ:
		case DFSM_AST_EXPRESSION_BINARY_AND:
		case DFSM_AST_EXPRESSION_BINARY_OR:
			/* Valid */
			break;
		default:
			g_assert_not_reached ();
	}

	g_assert (priv->left_node != NULL);
	dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->left_node));

	g_assert (priv->right_node != NULL);
	dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->right_node));
}

static void
dfsm_ast_expression_binary_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (node)->priv;

	dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->left_node), environment, error);

	if (*error != NULL) {
		return;
	}

	dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->right_node), environment, error);

	if (*error != NULL) {
		return;
	}
}

static void
dfsm_ast_expression_binary_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (node)->priv;
	GVariantType *lvalue_type, *rvalue_type;
	gboolean typechecks = FALSE;

	dfsm_ast_node_check (DFSM_AST_NODE (priv->left_node), environment, error);

	if (*error != NULL) {
		return;
	}

	dfsm_ast_node_check (DFSM_AST_NODE (priv->right_node), environment, error);

	if (*error != NULL) {
		return;
	}

	lvalue_type = dfsm_ast_expression_calculate_type (priv->left_node, environment);
	rvalue_type = dfsm_ast_expression_calculate_type (priv->right_node, environment);

	switch (priv->expression_type) {
		case DFSM_AST_EXPRESSION_BINARY_TIMES:
		case DFSM_AST_EXPRESSION_BINARY_DIVIDE:
		case DFSM_AST_EXPRESSION_BINARY_MODULUS:
		case DFSM_AST_EXPRESSION_BINARY_PLUS:
		case DFSM_AST_EXPRESSION_BINARY_MINUS:
		case DFSM_AST_EXPRESSION_BINARY_LT:
		case DFSM_AST_EXPRESSION_BINARY_LTE:
		case DFSM_AST_EXPRESSION_BINARY_GT:
		case DFSM_AST_EXPRESSION_BINARY_GTE:
			typechecks = (g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_BYTE) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_DOUBLE) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_INT16) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_INT32) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_INT64) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_UINT16) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_UINT32) == TRUE ||
			              g_variant_type_equal (lvalue_type, G_VARIANT_TYPE_UINT64) == TRUE) &&
			             (g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_BYTE) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_DOUBLE) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_INT16) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_INT32) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_INT64) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_UINT16) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_UINT32) == TRUE ||
			              g_variant_type_equal (rvalue_type, G_VARIANT_TYPE_UINT64) == TRUE);
		case DFSM_AST_EXPRESSION_BINARY_EQ:
		case DFSM_AST_EXPRESSION_BINARY_NEQ:
			typechecks = g_variant_type_equal (lvalue_type, rvalue_type);
			break;
		case DFSM_AST_EXPRESSION_BINARY_AND:
		case DFSM_AST_EXPRESSION_BINARY_OR:
			typechecks = g_variant_type_is_subtype_of (lvalue_type, G_VARIANT_TYPE_BOOLEAN) == TRUE &&
			             g_variant_type_is_subtype_of (rvalue_type, G_VARIANT_TYPE_BOOLEAN) == TRUE;
			break;
		default:
			g_assert_not_reached ();
	}

	if (typechecks == FALSE) {
		gchar *left, *right;

		left = g_variant_type_dup_string (lvalue_type);
		right = g_variant_type_dup_string (rvalue_type);

		g_variant_type_free (rvalue_type);
		g_variant_type_free (lvalue_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch between parameters to binary operator %u: received left type %s and right type %s.",
		             priv->expression_type, left, right);
		return;
	}

	g_variant_type_free (rvalue_type);
	g_variant_type_free (lvalue_type);
}

static GVariantType *
dfsm_ast_expression_binary_calculate_type (DfsmAstExpression *expression, DfsmEnvironment *environment)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (expression)->priv;

	switch (priv->expression_type) {
		/* Numeric operators */
		case DFSM_AST_EXPRESSION_BINARY_TIMES:
		case DFSM_AST_EXPRESSION_BINARY_DIVIDE:
		case DFSM_AST_EXPRESSION_BINARY_MODULUS:
		case DFSM_AST_EXPRESSION_BINARY_PLUS:
		case DFSM_AST_EXPRESSION_BINARY_MINUS:
			/* NOTE: We could come up with some fancy rules for type coercion to make everything safe.
			 * However, for simplicity's sake we currently just return the type of the left child expression. This can be changed in
			 * future if necessary. */
			return dfsm_ast_expression_calculate_type (priv->left_node, environment);
		/* Boolean relations */
		case DFSM_AST_EXPRESSION_BINARY_LT:
		case DFSM_AST_EXPRESSION_BINARY_LTE:
		case DFSM_AST_EXPRESSION_BINARY_GT:
		case DFSM_AST_EXPRESSION_BINARY_GTE:
		case DFSM_AST_EXPRESSION_BINARY_EQ:
		case DFSM_AST_EXPRESSION_BINARY_NEQ:
		/* Boolean operators */
		case DFSM_AST_EXPRESSION_BINARY_AND:
		case DFSM_AST_EXPRESSION_BINARY_OR:
			return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
		default:
			g_assert_not_reached ();
	}
}

static GVariant *
dfsm_ast_expression_binary_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (expression)->priv;
	GVariant *left_value, *right_value, *binary_value;
	GError *child_error = NULL;

	/* Evaluate our sub-expressions first. */
	left_value = dfsm_ast_expression_evaluate (priv->left_node, environment, &child_error);

	if (child_error != NULL) {
		g_assert (left_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	right_value = dfsm_ast_expression_evaluate (priv->right_node, environment, &child_error);

	if (child_error != NULL) {
		g_variant_unref (left_value);
		g_assert (right_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Do the actual evaluation. */
	switch (priv->expression_type) {
		/* Numeric operators */
		/* See the NOTE in dfsm_ast_expression_binary_calculate_type() for information about the poor coercion and type handling
		 * going on here. */
		#define NUMERIC_OPS(OP) { \
			const GVariantType *left_value_type; \
			\
			left_value_type = g_variant_get_type (left_value); \
			\
			if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_BYTE) == TRUE) { \
				binary_value = g_variant_new_byte (g_variant_get_byte (left_value) OP g_variant_get_byte (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_DOUBLE) == TRUE) { \
				binary_value = g_variant_new_double (g_variant_get_double (left_value) OP g_variant_get_double (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_INT16) == TRUE) { \
				binary_value = g_variant_new_int16 (g_variant_get_int16 (left_value) OP g_variant_get_int16 (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_INT32) == TRUE) { \
				binary_value = g_variant_new_int32 (g_variant_get_int32 (left_value) OP g_variant_get_int32 (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_INT64) == TRUE) { \
				binary_value = g_variant_new_int64 (g_variant_get_int64 (left_value) OP g_variant_get_int64 (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_UINT16) == TRUE) { \
				binary_value = g_variant_new_uint16 (g_variant_get_uint16 (left_value) OP g_variant_get_uint16 (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_UINT32) == TRUE) { \
				binary_value = g_variant_new_uint32 (g_variant_get_uint32 (left_value) OP g_variant_get_uint32 (right_value)); \
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_UINT64) == TRUE) { \
				binary_value = g_variant_new_uint64 (g_variant_get_uint64 (left_value) OP g_variant_get_uint64 (right_value)); \
			} else { \
				g_assert_not_reached (); \
			} \
			\
			break; \
		}
		case DFSM_AST_EXPRESSION_BINARY_TIMES:
			NUMERIC_OPS(*)
		case DFSM_AST_EXPRESSION_BINARY_DIVIDE:
			NUMERIC_OPS(/)
		case DFSM_AST_EXPRESSION_BINARY_MODULUS: {
			/* We can't use NUMERIC_OPS() here because modulus is defined in a shonky way for doubles. */
			const GVariantType *left_value_type;

			left_value_type = g_variant_get_type (left_value);

			if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_BYTE) == TRUE) {
				binary_value = g_variant_new_byte (g_variant_get_byte (left_value) % g_variant_get_byte (right_value));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_DOUBLE) == TRUE) {
				binary_value = g_variant_new_double (((guint64) g_variant_get_double (left_value)) %
				                                     ((guint64) g_variant_get_double (right_value)));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_INT16) == TRUE) {
				binary_value = g_variant_new_int16 (g_variant_get_int16 (left_value) % g_variant_get_int16 (right_value));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_INT32) == TRUE) {
				binary_value = g_variant_new_int32 (g_variant_get_int32 (left_value) % g_variant_get_int32 (right_value));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_INT64) == TRUE) {
				binary_value = g_variant_new_int64 (g_variant_get_int64 (left_value) % g_variant_get_int64 (right_value));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_UINT16) == TRUE) {
				binary_value = g_variant_new_uint16 (g_variant_get_uint16 (left_value) % g_variant_get_uint16 (right_value));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_UINT32) == TRUE) {
				binary_value = g_variant_new_uint32 (g_variant_get_uint32 (left_value) % g_variant_get_uint32 (right_value));
			} else if (g_variant_type_equal (left_value_type, G_VARIANT_TYPE_UINT64) == TRUE) {
				binary_value = g_variant_new_uint64 (g_variant_get_uint64 (left_value) % g_variant_get_uint64 (right_value));
			} else {
				g_assert_not_reached ();
			}

			break;
		}
		case DFSM_AST_EXPRESSION_BINARY_PLUS:
			NUMERIC_OPS(+)
		case DFSM_AST_EXPRESSION_BINARY_MINUS:
			NUMERIC_OPS(-)
		#undef NUMERIC_OPS
		/* Boolean relations */
		case DFSM_AST_EXPRESSION_BINARY_LT:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) < 0);
			break;
		case DFSM_AST_EXPRESSION_BINARY_LTE:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) <= 0);
			break;
		case DFSM_AST_EXPRESSION_BINARY_GT:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) > 0);
			break;
		case DFSM_AST_EXPRESSION_BINARY_GTE:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) >= 0);
			break;
		case DFSM_AST_EXPRESSION_BINARY_EQ:
			binary_value = g_variant_new_boolean (g_variant_equal (left_value, right_value));
			break;
		case DFSM_AST_EXPRESSION_BINARY_NEQ:
			binary_value = g_variant_new_boolean (!g_variant_equal (left_value, right_value));
			break;
		/* Boolean operators */
		case DFSM_AST_EXPRESSION_BINARY_AND:
			binary_value = g_variant_new_boolean (g_variant_get_boolean (left_value) && g_variant_get_boolean (right_value));
			break;
		case DFSM_AST_EXPRESSION_BINARY_OR:
			binary_value = g_variant_new_boolean (g_variant_get_boolean (left_value) || g_variant_get_boolean (right_value));
			break;
		default:
			g_assert_not_reached ();
	}

	/* Tidy up and return */
	g_variant_unref (right_value);
	g_variant_unref (left_value);

	g_assert (g_variant_is_floating (binary_value) == TRUE);
	g_variant_ref_sink (binary_value); /* sink reference */

	return binary_value;
}

static gdouble
dfsm_ast_expression_binary_calculate_weight (DfsmAstExpression *self)
{
	DfsmAstExpressionBinaryPrivate *priv = DFSM_AST_EXPRESSION_BINARY (self)->priv;

	return MAX (dfsm_ast_expression_calculate_weight (priv->left_node), dfsm_ast_expression_calculate_weight (priv->right_node));
}

/**
 * dfsm_ast_expression_binary_new:
 * @expression_type: the type of expression
 * @left_node: the expression's left node, or %NULL
 * @right_node: the expression's right node, or %NULL
 * @error: a #GError, or %NULL
 *
 * Create a new #DfsmAstExpression of type @expression_type with the given left and right nodes.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstExpression *
dfsm_ast_expression_binary_new (DfsmAstExpressionBinaryType expression_type, DfsmAstExpression *left_node, DfsmAstExpression *right_node, GError **error)
{
	DfsmAstExpressionBinary *expression;
	DfsmAstExpressionBinaryPrivate *priv;

	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (left_node), NULL);
	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (right_node), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	switch (expression_type) {
		case DFSM_AST_EXPRESSION_BINARY_TIMES:
		case DFSM_AST_EXPRESSION_BINARY_DIVIDE:
		case DFSM_AST_EXPRESSION_BINARY_MODULUS:
		case DFSM_AST_EXPRESSION_BINARY_PLUS:
		case DFSM_AST_EXPRESSION_BINARY_MINUS:
		case DFSM_AST_EXPRESSION_BINARY_LT:
		case DFSM_AST_EXPRESSION_BINARY_LTE:
		case DFSM_AST_EXPRESSION_BINARY_GT:
		case DFSM_AST_EXPRESSION_BINARY_GTE:
		case DFSM_AST_EXPRESSION_BINARY_EQ:
		case DFSM_AST_EXPRESSION_BINARY_NEQ:
		case DFSM_AST_EXPRESSION_BINARY_AND:
		case DFSM_AST_EXPRESSION_BINARY_OR:
			/* Valid */
			break;
		default:
			g_assert_not_reached ();
	}

	expression = g_object_new (DFSM_TYPE_AST_EXPRESSION_BINARY, NULL);
	priv = expression->priv;

	priv->expression_type = expression_type;
	priv->left_node = g_object_ref (left_node);
	priv->right_node = g_object_ref (right_node);

	return DFSM_AST_EXPRESSION (expression);
}
