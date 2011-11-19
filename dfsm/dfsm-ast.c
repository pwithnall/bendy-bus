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
#include <gio/gio.h>

#include "dfsm-ast.h"
#include "dfsm-parser.h"
#include "dfsm-utils.h"

/*
 * _dfsm_ast_node_init:
 * @node: a #DfsmAstNode
 * @node_type: the type of the node
 * @check_func: a function to check the node and its descendents are correct
 * @free_func: a function to free the node
 *
 * Initialise the members of the given @node.
 */
static void
_dfsm_ast_node_init (DfsmAstNode *node, DfsmAstNodeType node_type, DfsmAstNodeCheckFunc check_func, DfsmAstNodeFreeFunc free_func)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (check_func != NULL);
	g_return_if_fail (free_func != NULL);

	node->node_type = node_type;
	node->check_func = check_func;
	node->free_func = free_func;
	node->ref_count = 1;
}

/**
 * dfsm_ast_node_check:
 * @node: a #DfsmAstNode
 * @error: a #GError
 *
 * Check the node and its descendents are correct. This may, for example, involve type checking or checking of constants. If checking finds a problem,
 * @error will be set to a suitable #GError; otherwise, @error will remain unset.
 */
void
dfsm_ast_node_check (DfsmAstNode *node, GError **error)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	g_assert (node->check_func != NULL);
	node->check_func (node, error);
}

/**
 * dfsm_ast_node_ref:
 * @node: a #DfsmAstNode
 *
 * Increment the reference count of @node.
 */
gpointer
dfsm_ast_node_ref (gpointer/*<DfsmAstNode>*/ node)
{
	DfsmAstNode *_node;

	g_return_val_if_fail (node != NULL, NULL);

	_node = (DfsmAstNode*) node;
	g_atomic_int_inc (&(_node->ref_count));

	return node;
}

/**
 * dfsm_ast_node_unref:
 * @node: (allow-none): a #DfsmAstNode, or %NULL
 *
 * Decrement the reference count of @node if it's non-%NULL. If the reference count of the node reaches zero, the node will be destroyed.
 */
void
dfsm_ast_node_unref (gpointer/*<DfsmAstNode>*/ node)
{
	DfsmAstNode *_node = (DfsmAstNode*) node;

	if (_node != NULL && g_atomic_int_dec_and_test (&(_node->ref_count)) == TRUE) {
		g_assert (_node->free_func != NULL);
		_node->free_func (_node);
	}
}

static void
_dfsm_ast_object_check (DfsmAstNode *node, GError **error)
{
	DfsmAstObject *object = (DfsmAstObject*) node;
	guint i;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (object->parent.node_type == DFSM_AST_NODE_OBJECT);

	g_assert (object->object_path != NULL);
	g_assert (object->interface_names != NULL);
	g_assert (object->data_items != NULL);
	g_assert (object->states != NULL);

	for (i = 0; i < object->interface_names->len; i++) {
		const gchar *interface_name;

		interface_name = g_ptr_array_index (object->interface_names, i);
		g_assert (interface_name != NULL);
	}

	g_hash_table_iter_init (&iter, object->data_items);

	while (g_hash_table_iter_next (&iter, &key, &value) == TRUE) {
		g_assert (key != NULL);
		g_assert (value != NULL);
	}

	for (i = 0; i < object->states->len; i++) {
		const gchar *state_name;

		state_name = g_ptr_array_index (object->states, i);
		g_assert (state_name != NULL);
	}

	for (i = 0; i < object->transitions->len; i++) {
		DfsmAstTransition *transition;

		transition = g_ptr_array_index (object->transitions, i);
		g_assert (transition != NULL);
	}

	/* Conditions which may not hold as a result of invalid user input. */
	if (g_variant_is_object_path (object->object_path) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus object path: %s", object->object_path);
		return;
	}

	for (i = 0; i < object->interface_names->len; i++) {
		guint f;
		const gchar *interface_name;

		interface_name = g_ptr_array_index (object->interface_names, i);

		/* Valid interface name? */
		if (g_dbus_is_interface_name (interface_name) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus interface name: %s", interface_name);
			return;
		}

		/* Duplicates? */
		for (f = i + 1; f < object->interface_names->len; f++) {
			if (strcmp (interface_name, g_ptr_array_index (object->interface_names, f)) == 0) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate D-Bus interface name: %s",
				             interface_name);
				return;
			}
		}
	}

	g_hash_table_iter_init (&iter, object->data_items);

	while (g_hash_table_iter_next (&iter, &key, &value) == TRUE) {
		const gchar *variable_name;
		DfsmAstDataItem *data_item;

		variable_name = (const gchar*) key;
		data_item = (DfsmAstDataItem*) value;

		if (dfsm_is_variable_name (variable_name) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid variable name: %s", variable_name);
			return;
		}

		dfsm_ast_data_item_check (data_item, error);

		if (*error != NULL) {
			return;
		}
	}

	/* TODO: Assert that object explicitly lists data items for each of the properties of its interfaces. */

	if (object->states->len == 0) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "A default state is required.");
		return;
	}

	for (i = 0; i < object->states->len; i++) {
		guint f;
		const gchar *state_name;

		state_name = g_ptr_array_index (object->states, i);

		/* Valid state name? */
		if (dfsm_is_state_name (state_name) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid state name: %s", state_name);
			return;
		}

		/* Duplicates? */
		for (f = i + 1; f < object->states->len; f++) {
			if (strcmp (state_name, g_ptr_array_index (object->states, f)) == 0) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate state name: %s", state_name);
				return;
			}
		}
	}

	for (i = 0; i < object->transitions->len; i++) {
		DfsmAstTransition *transition;

		transition = g_ptr_array_index (object->transitions, i);

		dfsm_ast_node_check ((DfsmAstNode*) transition, error);

		if (*error != NULL) {
			return;
		}
	}
}

static void
_dfsm_ast_object_free (DfsmAstNode *node)
{
	DfsmAstObject *object = (DfsmAstObject*) node;

	if (object != NULL) {
		g_ptr_array_unref (object->transitions);
		g_ptr_array_unref (object->states);
		g_hash_table_unref (object->data_items);
		g_ptr_array_unref (object->interface_names);
		g_free (object->object_path);

		g_slice_free (DfsmAstObject, object);
	}
}

/**
 * dfsm_ast_object_new:
 * @object_path: the object's D-Bus path
 * @interface_names: an array of strings containing the names of the D-Bus interfaces implemented by the object
 * @data_blocks: an array of #GHashTable<!-- -->s containing the object-level variables in the object
 * @state_blocks: an array of #GPtrArray<!-- -->s of strings containing the names of the possible states of the object
 * @transition_blocks: an array of #DfsmAstTransition<!-- -->s containing the object's possible state transitions
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstObject AST node.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstObject *
dfsm_ast_object_new (const gchar *object_path, GPtrArray/*<string>*/ *interface_names, GPtrArray/*<GHashTable>*/ *data_blocks,
                     GPtrArray/*<GPtrArray>*/ *state_blocks, GPtrArray/*<DfsmAstTransition>*/ *transition_blocks, GError **error)
{
	DfsmAstObject *object;
	guint i;

	g_return_val_if_fail (object_path != NULL && *object_path != '\0', NULL);
	g_return_val_if_fail (interface_names != NULL, NULL);
	g_return_val_if_fail (data_blocks != NULL, NULL);
	g_return_val_if_fail (state_blocks != NULL, NULL);
	g_return_val_if_fail (transition_blocks != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	object = g_slice_new (DfsmAstObject);

	_dfsm_ast_node_init (&(object->parent), DFSM_AST_NODE_OBJECT, _dfsm_ast_object_check, _dfsm_ast_object_free);

	object->object_path = g_strdup (object_path);
	object->interface_names = g_ptr_array_ref (interface_names);
	object->data_items = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) dfsm_ast_data_item_free);
	object->states = g_ptr_array_new_with_free_func (g_free);
	object->transitions = g_ptr_array_ref (transition_blocks);

	/* Data items */
	for (i = 0; i < data_blocks->len; i++) {
		GHashTable *data_block;
		GHashTableIter iter;
		const gchar *key;
		const DfsmAstDataItem *value;

		data_block = g_ptr_array_index (data_blocks, i);
		g_hash_table_iter_init (&iter, data_block);

		while (g_hash_table_iter_next (&iter, (gpointer*) &key, (gpointer*) &value) == TRUE) {
			/* Check for duplicates */
			if (g_hash_table_lookup (object->data_items, key) != NULL) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate variable name: %s", key);
				return NULL; /* TODO: memory leak */
			}

			g_hash_table_insert (object->data_items, g_strdup (key), dfsm_ast_data_item_new (value->type_string, value->value_expression));
		}
	}

	/* States */
	for (i = 0; i < state_blocks->len; i++) {
		GPtrArray *states;
		guint f;

		states = g_ptr_array_index (state_blocks, i);

		for (f = 0; f < states->len; f++) {
			const gchar *state_name;
			guint g;

			state_name = g_ptr_array_index (states, f);

			/* Check for duplicates */
			for (g = 0; g < object->states->len; g++) {
				if (strcmp (state_name, g_ptr_array_index (object->states, g)) == 0) {
					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate state name: %s", state_name);
					return NULL; /* TODO: memory leak */
				}
			}

			g_ptr_array_add (object->states, g_strdup (state_name));
		}
	}

	return object;
}

/*
 * _dfsm_ast_expression_init:
 * @expression: a #DfsmAstExpression
 * @node_type: the type of the expression
 * @check_func: a function to check the node and its descendents
 * @free_func: a function to free the expression
 * @calculate_type_func: a function to calculate the type of the expression
 * @evaluate_func: a function to evaluate the expression
 *
 * Initialise the members of the given @expression.
 */
static void
_dfsm_ast_expression_init (DfsmAstExpression *expression, DfsmAstExpressionType expression_type, DfsmAstNodeCheckFunc check_func,
                           DfsmAstNodeFreeFunc free_func, DfsmAstExpressionCalculateTypeFunc calculate_type_func,
                           DfsmAstExpressionEvaluateFunc evaluate_func)
{
	g_return_if_fail (expression != NULL);

	_dfsm_ast_node_init (&(expression->parent), DFSM_AST_NODE_EXPRESSION, check_func, free_func);

	expression->expression_type = expression_type;
	expression->calculate_type_func = calculate_type_func;
	expression->evaluate_func = evaluate_func;
}

/**
 * dfsm_ast_expression_calculate_type:
 * @expression: a #DfsmAstExpression
 *
 * Calculate the type of the given @expression. In some cases this may not be a definite type, for example if the expression is an empty data
 * structure. In most cases, however, the type will be definite.
 *
 * This assumes that the expression has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the expression
 */
GVariantType *
dfsm_ast_expression_calculate_type (DfsmAstExpression *expression)
{
	g_return_val_if_fail (expression != NULL, NULL);

	g_assert (expression->calculate_type_func != NULL);
	return expression->calculate_type_func (expression);
}

/**
 * dfsm_ast_expression_evaluate:
 * @expression: a #DfsmAstExpression
 *
 * Evaluate the given @expression in the given @environment. This will not modify the environment.
 *
 * This assumes that the expression has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the expression
 */
GVariant *
dfsm_ast_expression_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	g_return_val_if_fail (expression != NULL, NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);

	g_assert (expression->evaluate_func != NULL);
	return expression->evaluate_func (expression, environment, error);
}

static void
_dfsm_ast_expression_function_call_check (DfsmAstNode *node, GError **error)
{
	DfsmAstExpressionFunctionCall *function_call = (DfsmAstExpressionFunctionCall*) node;
	const DfsmFunctionInfo *function_info;
	GVariantType *parameters_type;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (function_call->parent.parent.node_type == DFSM_AST_NODE_EXPRESSION);

	switch (function_call->parent.expression_type) {
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
			/* Valid */
			break;
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}

	g_assert (function_call->function_name != NULL);
	g_assert (function_call->parameters != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	function_info = dfsm_environment_get_function_info (function_call->function_name);

	if (function_info == NULL || dfsm_is_function_name (function_call->function_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid function name: %s", function_call->function_name);
		return;
	}

	dfsm_ast_node_check ((DfsmAstNode*) function_call->parameters, error);

	if (*error != NULL) {
		return;
	}

	parameters_type = dfsm_ast_expression_calculate_type (function_call->parameters);

	if (g_variant_type_is_subtype_of (parameters_type, function_info->parameters_type) == FALSE) {
		gchar *formal, *actual;

		formal = g_variant_type_dup_string (function_info->parameters_type);
		actual = g_variant_type_dup_string (parameters_type);

		g_variant_type_free (parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch between formal and actual parameters to function ‘%s’: expects type %s but received type %s.",
		             function_call->function_name, formal, actual);
		return;
	}

	g_variant_type_free (parameters_type);
}

static void
_dfsm_ast_expression_function_call_free (DfsmAstNode *node)
{
	DfsmAstExpressionFunctionCall *function_call = (DfsmAstExpressionFunctionCall*) node;

	if (function_call != NULL) {
		dfsm_ast_node_unref (function_call->parameters);
		g_free (function_call->function_name);

		g_slice_free (DfsmAstExpressionFunctionCall, function_call);
	}
}

static GVariantType *
_dfsm_ast_expression_function_call_calculate_type (DfsmAstExpression *expression)
{
	const DfsmFunctionInfo *function_info;
	DfsmAstExpressionFunctionCall *function_call = (DfsmAstExpressionFunctionCall*) expression;

	/* Type of the function call is just the return type of the function. */
	function_info = dfsm_environment_get_function_info (function_call->function_name);
	g_assert (function_info != NULL);

	return g_variant_type_copy (function_info->return_type);
}

static GVariant *
_dfsm_ast_expression_function_call_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	const DfsmFunctionInfo *function_info;
	GVariant *parameters_value, *function_call_value;
	DfsmAstExpressionFunctionCall *function_call = (DfsmAstExpressionFunctionCall*) expression;
	GError *child_error = NULL;

	/* Look up the function information. */
	function_info = dfsm_environment_get_function_info (function_call->function_name);
	g_assert (function_info != NULL);

	/* Evaluate the parameters. */
	parameters_value = dfsm_ast_expression_evaluate (function_call->parameters, environment, &child_error);

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
	DfsmAstExpressionFunctionCall *expression;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);
	g_return_val_if_fail (parameters != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	expression = g_slice_new (DfsmAstExpressionFunctionCall);

	_dfsm_ast_expression_init (&(expression->parent), DFSM_AST_EXPRESSION_FUNCTION_CALL, _dfsm_ast_expression_function_call_check,
	                           _dfsm_ast_expression_function_call_free, _dfsm_ast_expression_function_call_calculate_type,
	                           _dfsm_ast_expression_function_call_evaluate);

	expression->function_name = g_strdup (function_name);
	expression->parameters = dfsm_ast_node_ref (parameters);

	return (DfsmAstExpression*) expression;
}

static void
_dfsm_ast_expression_data_structure_check (DfsmAstNode *node, GError **error)
{
	DfsmAstExpressionDataStructure *data_structure = (DfsmAstExpressionDataStructure*) node;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (data_structure->parent.parent.node_type == DFSM_AST_NODE_EXPRESSION);

	switch (data_structure->parent.expression_type) {
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
			/* Valid */
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_NOT:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}

	g_assert (data_structure->data_structure != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	dfsm_ast_node_check ((DfsmAstNode*) data_structure->data_structure, error);

	if (*error != NULL) {
		return;
	}
}

static void
_dfsm_ast_expression_data_structure_free (DfsmAstNode *node)
{
	DfsmAstExpressionDataStructure *data_structure = (DfsmAstExpressionDataStructure*) node;

	if (data_structure != NULL) {
		dfsm_ast_node_unref (data_structure->data_structure);

		g_slice_free (DfsmAstExpressionDataStructure, data_structure);
	}
}

static GVariantType *
_dfsm_ast_expression_data_structure_calculate_type (DfsmAstExpression *expression)
{
	DfsmAstExpressionDataStructure *data_structure = (DfsmAstExpressionDataStructure*) expression;

	/* Type of the expression is the type of the data structure. */
	return dfsm_ast_data_structure_calculate_type (data_structure->data_structure, NULL /* TODO */);
}

static GVariant *
_dfsm_ast_expression_data_structure_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	DfsmAstExpressionDataStructure *data_structure = (DfsmAstExpressionDataStructure*) expression;

	return dfsm_ast_data_structure_to_variant (data_structure->data_structure, environment, error);
}

/**
 * dfsm_ast_expression_data_structure_new:
 * @data_structure: a #DfsmAstDataStructure to wrap
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstExpression wrapping the given @data_structure.
 *
 * Return value: (transfer full): a new expression
 */
DfsmAstExpression *
dfsm_ast_expression_data_structure_new (DfsmAstDataStructure *data_structure, GError **error)
{
	DfsmAstExpressionDataStructure *expression;

	g_return_val_if_fail (data_structure != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	expression = g_slice_new (DfsmAstExpressionDataStructure);

	_dfsm_ast_expression_init (&(expression->parent), DFSM_AST_EXPRESSION_DATA_STRUCTURE, _dfsm_ast_expression_data_structure_check,
	                           _dfsm_ast_expression_data_structure_free, _dfsm_ast_expression_data_structure_calculate_type,
	                           _dfsm_ast_expression_data_structure_evaluate);

	expression->data_structure = dfsm_ast_node_ref (data_structure);

	return (DfsmAstExpression*) expression;
}

static void
_dfsm_ast_expression_unary_check (DfsmAstNode *node, GError **error)
{
	DfsmAstExpressionUnary *unary = (DfsmAstExpressionUnary*) node;
	GVariantType *child_type;
	const GVariantType *desired_supertype;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (unary->parent.parent.node_type == DFSM_AST_NODE_EXPRESSION);

	switch (unary->parent.expression_type) {
		case DFSM_AST_EXPRESSION_NOT:
			/* Valid */
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}

	g_assert (unary->child_node != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	dfsm_ast_node_check ((DfsmAstNode*) unary->child_node, error);

	if (*error != NULL) {
		return;
	}

	child_type = dfsm_ast_expression_calculate_type (unary->child_node);

	switch (unary->parent.expression_type) {
		case DFSM_AST_EXPRESSION_NOT:
			desired_supertype = G_VARIANT_TYPE_BOOLEAN;
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}

	if (g_variant_type_is_subtype_of (child_type, desired_supertype) == FALSE) {
		gchar *formal, *actual;

		formal = g_variant_type_dup_string (desired_supertype);
		actual = g_variant_type_dup_string (child_type);

		g_variant_type_free (child_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch between the formal and actual parameter to unary operator %u: expects type %s but received type %s.",
		             unary->parent.expression_type, formal, actual);
		return;
	}

	g_variant_type_free (child_type);
}

static void
_dfsm_ast_expression_unary_free (DfsmAstNode *node)
{
	DfsmAstExpressionUnary *unary = (DfsmAstExpressionUnary*) node;

	if (unary != NULL) {
		dfsm_ast_node_unref (unary->child_node);

		g_slice_free (DfsmAstExpressionUnary, unary);
	}
}

static GVariantType *
_dfsm_ast_expression_unary_calculate_type (DfsmAstExpression *expression)
{
	DfsmAstExpressionUnary *unary = (DfsmAstExpressionUnary*) expression;

	switch (unary->parent.expression_type) {
		case DFSM_AST_EXPRESSION_NOT:
			return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}
}

static GVariant *
_dfsm_ast_expression_unary_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	GVariant *child_value, *unary_value;
	DfsmAstExpressionUnary *unary = (DfsmAstExpressionUnary*) expression;
	GError *child_error = NULL;

	/* Evaluate our sub-expression first. */
	child_value = dfsm_ast_expression_evaluate (unary->child_node, environment, &child_error);

	if (child_error != NULL) {
		g_assert (child_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Do the actual evaluation. */
	switch (unary->parent.expression_type) {
		case DFSM_AST_EXPRESSION_NOT:
			unary_value = g_variant_new_boolean (!g_variant_get_boolean (child_value));
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}

	/* Tidy up and return */
	g_variant_unref (child_value);

	return unary_value;
}

/**
 * dfsm_ast_expression_unary_new:
 * @expression_type: the type of expression
 * @child_node: the expression's child node, or %NULL
 * @error: a #GError, or %NULL
 *
 * Create a new #DfsmAstExpression of type @expression_type with the given child node.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstExpression *
dfsm_ast_expression_unary_new (DfsmAstExpressionType expression_type, DfsmAstExpression *child_node, GError **error)
{
	DfsmAstExpressionUnary *expression;

	g_return_val_if_fail (child_node != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	switch (expression_type) {
		case DFSM_AST_EXPRESSION_NOT:
			/* Valid */
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
		default:
			g_assert_not_reached ();
	}

	expression = g_slice_new (DfsmAstExpressionUnary);

	_dfsm_ast_expression_init (&(expression->parent), expression_type, _dfsm_ast_expression_unary_check, _dfsm_ast_expression_unary_free,
	                           _dfsm_ast_expression_unary_calculate_type, _dfsm_ast_expression_unary_evaluate);

	expression->child_node = dfsm_ast_node_ref (child_node);

	return (DfsmAstExpression*) expression;
}

static void
_dfsm_ast_expression_binary_check (DfsmAstNode *node, GError **error)
{
	DfsmAstExpressionBinary *binary = (DfsmAstExpressionBinary*) node;
	GVariantType *lvalue_type, *rvalue_type;
	gboolean typechecks = FALSE;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (binary->parent.parent.node_type == DFSM_AST_NODE_EXPRESSION);

	switch (binary->parent.expression_type) {
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
			/* Valid */
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
		default:
			g_assert_not_reached ();
	}

	g_assert (binary->left_node != NULL);
	g_assert (binary->right_node != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	dfsm_ast_node_check ((DfsmAstNode*) binary->left_node, error);

	if (*error != NULL) {
		return;
	}

	dfsm_ast_node_check ((DfsmAstNode*) binary->right_node, error);

	if (*error != NULL) {
		return;
	}

	lvalue_type = dfsm_ast_expression_calculate_type (binary->left_node);
	rvalue_type = dfsm_ast_expression_calculate_type (binary->right_node);

	switch (binary->parent.expression_type) {
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
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
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
			typechecks = g_variant_type_equal (lvalue_type, rvalue_type);
			break;
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
			typechecks = g_variant_type_is_subtype_of (lvalue_type, G_VARIANT_TYPE_BOOLEAN) == TRUE &&
			             g_variant_type_is_subtype_of (rvalue_type, G_VARIANT_TYPE_BOOLEAN) == TRUE;
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
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
		             binary->parent.expression_type, left, right);
		return;
	}

	g_variant_type_free (rvalue_type);
	g_variant_type_free (lvalue_type);
}

static void
_dfsm_ast_expression_binary_free (DfsmAstNode *node)
{
	DfsmAstExpressionBinary *binary = (DfsmAstExpressionBinary*) node;

	if (binary != NULL) {
		dfsm_ast_node_unref (binary->right_node);
		dfsm_ast_node_unref (binary->left_node);

		g_slice_free (DfsmAstExpressionBinary, binary);
	}
}

static GVariantType *
_dfsm_ast_expression_binary_calculate_type (DfsmAstExpression *expression)
{
	DfsmAstExpressionBinary *binary = (DfsmAstExpressionBinary*) expression;

	switch (binary->parent.expression_type) {
		/* Numeric operators */
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
			/* NOTE: We could come up with some fancy rules for type coercion to make everything safe.
			 * However, for simplicity's sake we currently just return the type of the left child expression. This can be changed in
			 * future if necessary. */
			return dfsm_ast_expression_calculate_type (binary->left_node);
		/* Boolean relations */
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		/* Boolean operators */
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
			return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
		default:
			g_assert_not_reached ();
	}
}

static GVariant *
_dfsm_ast_expression_binary_evaluate (DfsmAstExpression *expression, DfsmEnvironment *environment, GError **error)
{
	GVariant *left_value, *right_value, *binary_value;
	DfsmAstExpressionBinary *binary = (DfsmAstExpressionBinary*) expression;
	GError *child_error = NULL;

	/* Evaluate our sub-expressions first. */
	left_value = dfsm_ast_expression_evaluate (binary->left_node, environment, &child_error);

	if (child_error != NULL) {
		g_assert (left_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	right_value = dfsm_ast_expression_evaluate (binary->right_node, environment, &child_error);

	if (child_error != NULL) {
		g_variant_unref (left_value);
		g_assert (right_value == NULL);

		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Do the actual evaluation. */
	switch (binary->parent.expression_type) {
		/* Numeric operators */
		/* See the NOTE in _dfsm_ast_expression_binary_calculate_type() for information about the poor coercion and type handling
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
		case DFSM_AST_EXPRESSION_TIMES:
			NUMERIC_OPS(*)
		case DFSM_AST_EXPRESSION_DIVIDE:
			NUMERIC_OPS(/)
		case DFSM_AST_EXPRESSION_MODULUS: {
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
		case DFSM_AST_EXPRESSION_PLUS:
			NUMERIC_OPS(+)
		case DFSM_AST_EXPRESSION_MINUS:
			NUMERIC_OPS(-)
		#undef NUMERIC_OPS
		/* Boolean relations */
		case DFSM_AST_EXPRESSION_LT:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) < 0);
			break;
		case DFSM_AST_EXPRESSION_LTE:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) <= 0);
			break;
		case DFSM_AST_EXPRESSION_GT:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) > 0);
			break;
		case DFSM_AST_EXPRESSION_GTE:
			binary_value = g_variant_new_boolean (g_variant_compare (left_value, right_value) >= 0);
			break;
		case DFSM_AST_EXPRESSION_EQ:
			binary_value = g_variant_new_boolean (g_variant_equal (left_value, right_value));
			break;
		case DFSM_AST_EXPRESSION_NEQ:
			binary_value = g_variant_new_boolean (!g_variant_equal (left_value, right_value));
			break;
		/* Boolean operators */
		case DFSM_AST_EXPRESSION_AND:
			binary_value = g_variant_new_boolean (g_variant_get_boolean (left_value) && g_variant_get_boolean (right_value));
			break;
		case DFSM_AST_EXPRESSION_OR:
			binary_value = g_variant_new_boolean (g_variant_get_boolean (left_value) || g_variant_get_boolean (right_value));
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
		default:
			g_assert_not_reached ();
	}

	/* Tidy up and return */
	g_variant_unref (right_value);
	g_variant_unref (left_value);

	return binary_value;
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
dfsm_ast_expression_binary_new (DfsmAstExpressionType expression_type, DfsmAstExpression *left_node, DfsmAstExpression *right_node, GError **error)
{
	DfsmAstExpressionBinary *expression;

	g_return_val_if_fail (left_node != NULL, NULL);
	g_return_val_if_fail (right_node != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	switch (expression_type) {
		case DFSM_AST_EXPRESSION_TIMES:
		case DFSM_AST_EXPRESSION_DIVIDE:
		case DFSM_AST_EXPRESSION_MODULUS:
		case DFSM_AST_EXPRESSION_PLUS:
		case DFSM_AST_EXPRESSION_MINUS:
		case DFSM_AST_EXPRESSION_LT:
		case DFSM_AST_EXPRESSION_LTE:
		case DFSM_AST_EXPRESSION_GT:
		case DFSM_AST_EXPRESSION_GTE:
		case DFSM_AST_EXPRESSION_EQ:
		case DFSM_AST_EXPRESSION_NEQ:
		case DFSM_AST_EXPRESSION_AND:
		case DFSM_AST_EXPRESSION_OR:
			/* Valid */
			break;
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
		default:
			g_assert_not_reached ();
	}

	expression = g_slice_new (DfsmAstExpressionBinary);

	_dfsm_ast_expression_init (&(expression->parent), expression_type, _dfsm_ast_expression_binary_check, _dfsm_ast_expression_binary_free,
	                           _dfsm_ast_expression_binary_calculate_type, _dfsm_ast_expression_binary_evaluate);

	expression->left_node = dfsm_ast_node_ref (left_node);
	expression->right_node = dfsm_ast_node_ref (right_node);

	return (DfsmAstExpression*) expression;
}

/**
 * dfsm_ast_dictionary_entry_new:
 * @key: expression giving entry's key
 * @value: expression giving entry's value
 *
 * Create a new #DfsmAstDictionaryEntry mapping the given @key to the given @value.
 *
 * Return value: (transfer full): a new dictionary entry
 */
DfsmAstDictionaryEntry *
dfsm_ast_dictionary_entry_new (DfsmAstExpression *key, DfsmAstExpression *value)
{
	DfsmAstDictionaryEntry *entry;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (value != NULL, NULL);

	entry = g_slice_new (DfsmAstDictionaryEntry);

	entry->key = dfsm_ast_node_ref (key);
	entry->value = dfsm_ast_node_ref (value);

	return entry;
}

/**
 * dfsm_ast_dictionary_entry_free:
 * @entry: entry to free
 *
 * Free the given dictionary entry.
 */
void
dfsm_ast_dictionary_entry_free (DfsmAstDictionaryEntry *entry)
{
	if (entry != NULL) {
		dfsm_ast_node_unref (entry->value);
		dfsm_ast_node_unref (entry->key);

		g_slice_free (DfsmAstDictionaryEntry, entry);
	}
}

static void
_dfsm_ast_data_structure_check (DfsmAstNode *node, GError **error)
{
	guint i;
	DfsmAstDataStructure *data_structure = (DfsmAstDataStructure*) node;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (data_structure->parent.node_type == DFSM_AST_NODE_DATA_STRUCTURE);

	switch (data_structure->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_STRING:
			g_assert (data_structure->string_val != NULL);
			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			g_assert (data_structure->object_path_val != NULL);
			break;
		case DFSM_AST_DATA_SIGNATURE:
			g_assert (data_structure->signature_val != NULL);
			break;
		case DFSM_AST_DATA_ARRAY:
			g_assert (data_structure->array_val != NULL);

			for (i = 0; i < data_structure->array_val->len; i++) {
				g_assert (g_ptr_array_index (data_structure->array_val, i) != NULL);
			}

			break;
		case DFSM_AST_DATA_STRUCT:
			g_assert (data_structure->struct_val != NULL);

			for (i = 0; i < data_structure->struct_val->len; i++) {
				g_assert (g_ptr_array_index (data_structure->struct_val, i) != NULL);
			}

			break;
		case DFSM_AST_DATA_VARIANT:
			g_assert (data_structure->variant_val != NULL);
			break;
		case DFSM_AST_DATA_DICT:
			g_assert (data_structure->dict_val != NULL);

			for (i = 0; i < data_structure->dict_val->len; i++) {
				g_assert (g_ptr_array_index (data_structure->dict_val, i) != NULL);
			}

			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_REGEXP:
			g_assert (data_structure->regexp_val != NULL);
			break;
		case DFSM_AST_DATA_VARIABLE:
			g_assert (data_structure->variable_val != NULL);
			break;
		default:
			g_assert_not_reached ();
	}

	/* Conditions which may not hold as a result of invalid user input. */
	switch (data_structure->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_STRING:
			/* Valid UTF-8? */
			if (g_utf8_validate (data_structure->string_val, -1, NULL) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Invalid UTF-8 in string: %s", data_structure->string_val);
				return;
			}

			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			/* Valid object path? */
			if (g_variant_is_object_path (data_structure->object_path_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Invalid D-Bus object path: %s", data_structure->object_path_val);
				return;
			}

			break;
		case DFSM_AST_DATA_SIGNATURE:
			/* Valid signature? */
			if (g_variant_type_string_is_valid ((gchar*) data_structure->signature_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Invalid D-Bus type signature: %s", data_structure->signature_val);
				return;
			}

			break;
		case DFSM_AST_DATA_ARRAY: {
			GVariantType *expected_type = NULL;

			/* All entries valid and of the same type? */
			for (i = 0; i < data_structure->array_val->len; i++) {
				GVariantType *child_type;
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (data_structure->array_val, i);

				/* Valid expression? */
				dfsm_ast_node_check ((DfsmAstNode*) expr, error);

				if (*error != NULL) {
					goto array_error;
				}

				/* Equal type? */
				child_type = dfsm_ast_expression_calculate_type (expr);

				if (expected_type == NULL) {
					/* First iteration */
					expected_type = g_variant_type_copy (child_type);
				} else if (g_variant_type_equal (expected_type, child_type) == FALSE) {
					gchar *expected, *received;

					expected = g_variant_type_dup_string (expected_type);
					received = g_variant_type_dup_string (child_type);

					g_variant_type_free (child_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             "Type mismatch between elements in array: expected type %s but received type %s.",
					             expected, received);

					goto array_error;
				}

				g_variant_type_free (child_type);
			}

		array_error:
			g_variant_type_free (expected_type);

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			/* All entries valid? */
			for (i = 0; i < data_structure->struct_val->len; i++) {
				DfsmAstExpression *expr;

				expr = g_ptr_array_index (data_structure->struct_val, i);

				dfsm_ast_node_check ((DfsmAstNode*) expr, error);

				if (*error != NULL) {
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_VARIANT:
			if (g_variant_is_normal_form (data_structure->variant_val) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid variant: not in normal form");
				return;
			}

			break;
		case DFSM_AST_DATA_DICT: {
			GVariantType *expected_key_type = NULL, *expected_value_type = NULL;

			/* All entries valid with no duplicate keys and equal types for all keys and values? */
			for (i = 0; i < data_structure->dict_val->len; i++) {
				GVariantType *key_type, *value_type;
				DfsmAstDictionaryEntry *entry;

				/* Valid expressions? */
				entry = g_ptr_array_index (data_structure->dict_val, i);

				dfsm_ast_node_check ((DfsmAstNode*) entry->key, error);

				if (*error != NULL) {
					goto dict_error;
				}

				dfsm_ast_node_check ((DfsmAstNode*) entry->value, error);

				if (*error != NULL) {
					goto dict_error;
				}

				/* Equal types? */
				key_type = dfsm_ast_expression_calculate_type (entry->key);
				value_type = dfsm_ast_expression_calculate_type (entry->value);

				g_assert ((expected_key_type == NULL) == (expected_value_type == NULL));

				if (expected_key_type == NULL) {
					/* First iteration */
					expected_key_type = g_variant_type_copy (key_type);
					expected_value_type = g_variant_type_copy (value_type);
				} else if (g_variant_type_equal (expected_key_type, key_type) == FALSE) {
					gchar *expected, *received;

					expected = g_variant_type_dup_string (expected_key_type);
					received = g_variant_type_dup_string (key_type);

					g_variant_type_free (key_type);
					g_variant_type_free (value_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             "Type mismatch between keys in dictionary: expected type %s but received type %s.",
					             expected, received);

					goto dict_error;
				} else if (g_variant_type_equal (expected_value_type, value_type) == FALSE) {
					gchar *expected, *received;

					expected = g_variant_type_dup_string (expected_value_type);
					received = g_variant_type_dup_string (value_type);

					g_variant_type_free (key_type);
					g_variant_type_free (value_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             "Type mismatch between values in dictionary: expected type %s but received type %s.",
					             expected, received);

					goto dict_error;
				}

				g_variant_type_free (key_type);
				g_variant_type_free (value_type);
			}

		dict_error:
			g_variant_type_free (expected_key_type);
			g_variant_type_free (expected_value_type);

			break;
		}
		case DFSM_AST_DATA_UNIX_FD:
			/* Nothing to do here */
			break;
		case DFSM_AST_DATA_REGEXP: {
			/* Check if the regexp is valid by trying to parse it */
			GRegex *regex = g_regex_new (data_structure->regexp_val, 0, 0, error);

			if (regex != NULL) {
				g_regex_unref (regex);
			}

			if (*error != NULL) {
				return;
			}
		}
		case DFSM_AST_DATA_VARIABLE:
			/* Valid variable? */
			dfsm_ast_node_check ((DfsmAstNode*) data_structure->variable_val, error);

			if (*error != NULL) {
				return;
			}
		default:
			g_assert_not_reached ();
	}
}

static void
_dfsm_ast_data_structure_free (DfsmAstNode *node)
{
	DfsmAstDataStructure *data_structure = (DfsmAstDataStructure*) node;

	if (data_structure != NULL) {
		switch (data_structure->data_structure_type) {
			case DFSM_AST_DATA_BYTE:
			case DFSM_AST_DATA_BOOLEAN:
			case DFSM_AST_DATA_INT16:
			case DFSM_AST_DATA_UINT16:
			case DFSM_AST_DATA_INT32:
			case DFSM_AST_DATA_UINT32:
			case DFSM_AST_DATA_INT64:
			case DFSM_AST_DATA_UINT64:
			case DFSM_AST_DATA_DOUBLE:
				/* Nothing to free here */
				break;
			case DFSM_AST_DATA_STRING:
				g_free (data_structure->string_val);
				break;
			case DFSM_AST_DATA_OBJECT_PATH:
				g_free (data_structure->object_path_val);
				break;
			case DFSM_AST_DATA_SIGNATURE:
				g_free (data_structure->signature_val);
				break;
			case DFSM_AST_DATA_ARRAY:
				g_ptr_array_unref (data_structure->array_val);
				break;
			case DFSM_AST_DATA_STRUCT:
				g_ptr_array_unref (data_structure->struct_val);
				break;
			case DFSM_AST_DATA_VARIANT:
				g_variant_unref (data_structure->variant_val);
				break;
			case DFSM_AST_DATA_DICT:
				g_ptr_array_unref (data_structure->dict_val);
				break;
			case DFSM_AST_DATA_UNIX_FD:
				/* Nothing to free here */
				break;
			case DFSM_AST_DATA_REGEXP:
				g_free (data_structure->regexp_val);
				break;
			case DFSM_AST_DATA_VARIABLE:
				dfsm_ast_node_unref (data_structure->variable_val);
				break;
			default:
				g_assert_not_reached ();
		}

		g_slice_free (DfsmAstDataStructure, data_structure);
	}
}

/**
 * dfsm_ast_data_structure_new:
 * @data_structure_type: the type of the outermost data structure
 * @value: value of the data structure
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstDataStructure of type @data_structure_type and containing the given @value.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstDataStructure *
dfsm_ast_data_structure_new (DfsmAstDataStructureType data_structure_type, gpointer value, GError **error)
{
	DfsmAstDataStructure *data_structure;

	g_return_val_if_fail (value != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	data_structure = g_slice_new (DfsmAstDataStructure);

	_dfsm_ast_node_init (&(data_structure->parent), DFSM_AST_NODE_DATA_STRUCTURE, _dfsm_ast_data_structure_check, _dfsm_ast_data_structure_free);

	switch (data_structure_type) {
		case DFSM_AST_DATA_BYTE:
			data_structure->byte_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_BOOLEAN:
			data_structure->boolean_val = (GPOINTER_TO_UINT (value) == 1) ? TRUE : FALSE;
			break;
		case DFSM_AST_DATA_INT16:
			data_structure->int16_val = *((gint64*) value);
			break;
		case DFSM_AST_DATA_UINT16:
			data_structure->uint16_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_INT32:
			data_structure->int32_val = *((gint64*) value);
			break;
		case DFSM_AST_DATA_UINT32:
			data_structure->uint32_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_INT64:
			data_structure->int64_val = *((gint64*) value);
			break;
		case DFSM_AST_DATA_UINT64:
			data_structure->uint64_val = *((guint64*) value);
			break;
		case DFSM_AST_DATA_DOUBLE:
			data_structure->double_val = *((gdouble*) value);
			break;
		case DFSM_AST_DATA_STRING:
			data_structure->string_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_OBJECT_PATH:
			data_structure->object_path_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_SIGNATURE:
			data_structure->signature_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_ARRAY:
			data_structure->array_val = g_ptr_array_ref (value); /* array of DfsmAstExpressions */
			break;
		case DFSM_AST_DATA_STRUCT:
			data_structure->struct_val = g_ptr_array_ref (value); /* array of DfsmAstExpressions */
			break;
		case DFSM_AST_DATA_VARIANT:
			/* Note: not representable in the FSM language. */
			data_structure->variant_val = NULL;
			break;
		case DFSM_AST_DATA_DICT:
			data_structure->dict_val = g_ptr_array_ref (value); /* array of DfsmAstDictionaryEntrys */
			break;
		case DFSM_AST_DATA_UNIX_FD:
			/* Note: not representable in the FSM language. */
			data_structure->unix_fd_val = 0;
			break;
		case DFSM_AST_DATA_REGEXP:
			data_structure->regexp_val = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_VARIABLE:
			data_structure->variable_val = dfsm_ast_node_ref (value); /* DfsmAstVariable */
			break;
		default:
			g_assert_not_reached ();
	}

	data_structure->data_structure_type = data_structure_type;

	return data_structure;
}

/**
 * dfsm_ast_data_structure_calculate_type:
 * @data_structure: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 *
 * Calculate the type of the given @data_structure. In some cases this may not be a definite type, for example if the data structure is an empty
 * array, struct or dictionary. In most cases, however, the type will be definite.
 *
 * This assumes that the data structure has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the data structure
 */
GVariantType *
dfsm_ast_data_structure_calculate_type (DfsmAstDataStructure *data_structure, DfsmEnvironment *environment)
{
	g_return_val_if_fail (data_structure != NULL, NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	switch (data_structure->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
			return g_variant_type_copy (G_VARIANT_TYPE_BYTE);
		case DFSM_AST_DATA_BOOLEAN:
			return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
		case DFSM_AST_DATA_INT16:
			return g_variant_type_copy (G_VARIANT_TYPE_INT16);
		case DFSM_AST_DATA_UINT16:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT16);
		case DFSM_AST_DATA_INT32:
			return g_variant_type_copy (G_VARIANT_TYPE_INT32);
		case DFSM_AST_DATA_UINT32:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
		case DFSM_AST_DATA_INT64:
			return g_variant_type_copy (G_VARIANT_TYPE_INT64);
		case DFSM_AST_DATA_UINT64:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT64);
		case DFSM_AST_DATA_DOUBLE:
			return g_variant_type_copy (G_VARIANT_TYPE_DOUBLE);
		case DFSM_AST_DATA_STRING:
			return g_variant_type_copy (G_VARIANT_TYPE_STRING);
		case DFSM_AST_DATA_OBJECT_PATH:
			return g_variant_type_copy (G_VARIANT_TYPE_OBJECT_PATH);
		case DFSM_AST_DATA_SIGNATURE:
			return g_variant_type_copy (G_VARIANT_TYPE_SIGNATURE);
		case DFSM_AST_DATA_ARRAY: {
			GVariantType *array_type, *child_type;

			/* Empty arrays need special-casing. */
			if (data_structure->array_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_ARRAY);
			}

			/* Having checked the array already, we can assume all elements are of the same type. */
			child_type = dfsm_ast_expression_calculate_type (g_ptr_array_index (data_structure->array_val, 0));
			array_type = g_variant_type_new_array (child_type);
			g_variant_type_free (child_type);

			return array_type;
		}
		case DFSM_AST_DATA_STRUCT: {
			GVariantType *struct_type;
			GPtrArray/*<GVariantType>*/ *child_types;
			guint i;

			/* Empty structs need special-casing. */
			if (data_structure->struct_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_TUPLE);
			}

			/* Build an array of the types of the struct elements. */
			child_types = g_ptr_array_new_with_free_func ((GDestroyNotify) g_variant_type_free);

			for (i = 0; i < data_structure->struct_val->len; i++) {
				g_ptr_array_add (child_types, dfsm_ast_expression_calculate_type (g_ptr_array_index (data_structure->struct_val, i)));
			}

			struct_type = g_variant_type_new_tuple ((const GVariantType**) child_types->pdata, child_types->len);
			g_ptr_array_unref (child_types);

			return struct_type;
		}
		case DFSM_AST_DATA_VARIANT:
			return g_variant_type_copy (G_VARIANT_TYPE_VARIANT);
		case DFSM_AST_DATA_DICT: {
			DfsmAstDictionaryEntry *entry;
			GVariantType *dict_type, *entry_type, *key_type, *value_type;

			/* Empty dictionaries need special-casing. */
			if (data_structure->dict_val->len == 0) {
				return g_variant_type_copy (G_VARIANT_TYPE_DICTIONARY);
			}

			/* Otherwise, we assume that all entries have the same type (since we checked this before). */
			entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (data_structure->dict_val, 0);

			key_type = dfsm_ast_expression_calculate_type (entry->key);
			value_type = dfsm_ast_expression_calculate_type (entry->value);
			entry_type = g_variant_type_new_dict_entry (key_type, value_type);
			g_variant_type_free (value_type);
			g_variant_type_free (key_type);

			dict_type = g_variant_type_new_array (entry_type);

			g_variant_type_free (entry_type);

			return dict_type;
		}
		case DFSM_AST_DATA_UNIX_FD:
			return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
		case DFSM_AST_DATA_REGEXP:
			return g_variant_type_copy (G_VARIANT_TYPE_STRING);
		case DFSM_AST_DATA_VARIABLE:
			return dfsm_ast_variable_calculate_type (data_structure->variable_val, environment);
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_data_structure_to_variant:
 * @data_structure: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 * @error: (allow-none): a #GError, or %NULL
 *
 * Convert the data structure given by @data_structure to a #GVariant in the given @environment.
 *
 * This assumes that the @data_structure has been successfully checked by dfsm_ast_node_check() beforehand. It is an error to call this function
 * otherwise.
 *
 * Return value: (transfer full): the #GVariant representation of the data structure
 */
GVariant *
dfsm_ast_data_structure_to_variant (DfsmAstDataStructure *data_structure, DfsmEnvironment *environment, GError **error)
{
	g_return_val_if_fail (data_structure != NULL, NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	switch (data_structure->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
			return g_variant_new_byte (data_structure->byte_val);
		case DFSM_AST_DATA_BOOLEAN:
			return g_variant_new_boolean (data_structure->boolean_val);
		case DFSM_AST_DATA_INT16:
			return g_variant_new_int16 (data_structure->int16_val);
		case DFSM_AST_DATA_UINT16:
			return g_variant_new_uint16 (data_structure->uint16_val);
		case DFSM_AST_DATA_INT32:
			return g_variant_new_int32 (data_structure->int32_val);
		case DFSM_AST_DATA_UINT32:
			return g_variant_new_uint32 (data_structure->uint32_val);
		case DFSM_AST_DATA_INT64:
			return g_variant_new_int64 (data_structure->int64_val);
		case DFSM_AST_DATA_UINT64:
			return g_variant_new_uint64 (data_structure->uint64_val);
		case DFSM_AST_DATA_DOUBLE:
			return g_variant_new_double (data_structure->double_val);
		case DFSM_AST_DATA_STRING:
			return g_variant_new_string (data_structure->string_val);
		case DFSM_AST_DATA_OBJECT_PATH:
			return g_variant_new_object_path (data_structure->object_path_val);
		case DFSM_AST_DATA_SIGNATURE:
			return g_variant_new_signature (data_structure->signature_val);
		case DFSM_AST_DATA_ARRAY: {
			GVariantBuilder builder;
			guint i;

			g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

			for (i = 0; i < data_structure->array_val->len; i++) {
				GVariant *child_value;
				DfsmAstExpression *child_expression;
				GError *child_error = NULL;

				/* Evaluate the child expression to get a GVariant value. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (data_structure->array_val, i);
				child_value = dfsm_ast_expression_evaluate (child_expression, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return NULL;
				}

				/* Add it to the growing GVariant array. */
				g_variant_builder_add_value (&builder, child_value);

				g_variant_unref (child_value);
			}

			return g_variant_builder_end (&builder);
		}
		case DFSM_AST_DATA_STRUCT: {
			GVariantBuilder builder;
			guint i;

			g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);

			for (i = 0; i < data_structure->struct_val->len; i++) {
				GVariant *child_value;
				DfsmAstExpression *child_expression;
				GError *child_error = NULL;

				/* Evaluate the child expression to get a GVariant value. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (data_structure->struct_val, i);
				child_value = dfsm_ast_expression_evaluate (child_expression, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return NULL;
				}

				/* Add it to the growing GVariant struct. */
				g_variant_builder_add_value (&builder, child_value);

				g_variant_unref (child_value);
			}

			return g_variant_builder_end (&builder);
		}
		case DFSM_AST_DATA_VARIANT:
			return g_variant_new_variant (data_structure->variant_val);
		case DFSM_AST_DATA_DICT: {
			GVariantBuilder builder;
			guint i;

			g_variant_builder_init (&builder, G_VARIANT_TYPE_DICTIONARY);

			for (i = 0; i < data_structure->dict_val->len; i++) {
				GVariant *key_value, *value_value;
				DfsmAstDictionaryEntry *dict_entry;
				GError *child_error = NULL;

				/* Evaluate the child expressions to get GVariant values. */
				dict_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (data_structure->dict_val, i);

				key_value = dfsm_ast_expression_evaluate (dict_entry->key, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return NULL;
				}

				value_value = dfsm_ast_expression_evaluate (dict_entry->value, environment, &child_error);

				if (child_error != NULL) {
					/* Error! */
					g_variant_unref (key_value);
					g_propagate_error (error, child_error);
					return NULL;
				}

				/* Add them to the growing GVariant dict. */
				g_variant_builder_open (&builder, G_VARIANT_TYPE_DICT_ENTRY);
				g_variant_builder_add_value (&builder, key_value);
				g_variant_builder_add_value (&builder, value_value);
				g_variant_builder_close (&builder);

				g_variant_unref (value_value);
				g_variant_unref (key_value);
			}

			return g_variant_builder_end (&builder);
		}
		case DFSM_AST_DATA_UNIX_FD:
			return g_variant_new_uint32 (data_structure->unix_fd_val);
		case DFSM_AST_DATA_REGEXP:
			return g_variant_new_string (data_structure->regexp_val);
		case DFSM_AST_DATA_VARIABLE:
			return dfsm_ast_variable_to_variant (data_structure->variable_val, environment, error);
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_data_structure_set_from_variant:
 * @data_structure: a #DfsmAstDataStructure
 * @environment: a #DfsmEnvironment containing all variables
 * @new_value: the #GVariant value to assign to @data_structure
 * @error: (allow-none): a #GError, or %NULL
 *
 * Set the given @data_structure's value in @environment to the #GVariant value given in @new_value. This will recursively assign to child data
 * structures inside the data structure (e.g. if the data structure is an array of variables, each of the variables will be assigned to).
 *
 * It's an error to call this function with a data structure which isn't comprised entirely of variables or structures of them. Similarly, it's an
 * error to call this function with a @new_value which doesn't match the data structure's type and number of elements.
 */
void
dfsm_ast_data_structure_set_from_variant (DfsmAstDataStructure *data_structure, DfsmEnvironment *environment, GVariant *new_value, GError **error)
{
	g_return_if_fail (data_structure != NULL);
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (new_value != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	switch (data_structure->data_structure_type) {
		case DFSM_AST_DATA_BYTE:
		case DFSM_AST_DATA_BOOLEAN:
		case DFSM_AST_DATA_INT16:
		case DFSM_AST_DATA_UINT16:
		case DFSM_AST_DATA_INT32:
		case DFSM_AST_DATA_UINT32:
		case DFSM_AST_DATA_INT64:
		case DFSM_AST_DATA_UINT64:
		case DFSM_AST_DATA_DOUBLE:
		case DFSM_AST_DATA_STRING:
		case DFSM_AST_DATA_OBJECT_PATH:
		case DFSM_AST_DATA_SIGNATURE:
		case DFSM_AST_DATA_VARIANT:
		case DFSM_AST_DATA_UNIX_FD:
		case DFSM_AST_DATA_REGEXP:
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid assignment to a basic data structure.");
			break;
		case DFSM_AST_DATA_ARRAY: {
			guint i;

			/* We can safely assume that the array and the variant have the same type, length, etc. since that's all been checked
			 * before. */
			for (i = 0; i < data_structure->array_val->len; i++) {
				GVariant *child_variant;
				DfsmAstExpression *child_expression;
				DfsmAstDataStructure *child_data_structure;
				GError *child_error = NULL;

				/* TODO: For the moment, we hackily assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and
				 * extract its data structure to recursively assign to. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (data_structure->array_val, i);
				g_assert (child_expression->expression_type == DFSM_AST_EXPRESSION_DATA_STRUCTURE);
				child_data_structure = ((DfsmAstExpressionDataStructure*) child_expression)->data_structure;

				/* Get the child variant. */
				child_variant = g_variant_get_child_value (new_value, i);

				/* Recursively assign to the child data structure. */
				dfsm_ast_data_structure_set_from_variant (child_data_structure, environment, child_variant, &child_error);

				g_variant_unref (child_variant);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_STRUCT: {
			guint i;

			/* We can safely assume that the struct and the variant have the same type, etc. since that's all been checked before. */
			for (i = 0; i < data_structure->struct_val->len; i++) {
				GVariant *child_variant;
				DfsmAstExpression *child_expression;
				DfsmAstDataStructure *child_data_structure;
				GError *child_error = NULL;

				/* TODO: For the moment, we hackily assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and
				 * extract its data structure to recursively assign to. */
				child_expression = (DfsmAstExpression*) g_ptr_array_index (data_structure->struct_val, i);
				g_assert (child_expression->expression_type == DFSM_AST_EXPRESSION_DATA_STRUCTURE);
				child_data_structure = ((DfsmAstExpressionDataStructure*) child_expression)->data_structure;

				/* Get the child variant. */
				child_variant = g_variant_get_child_value (new_value, i);

				/* Recursively assign to the child data structure. */
				dfsm_ast_data_structure_set_from_variant (child_data_structure, environment, child_variant, &child_error);

				g_variant_unref (child_variant);

				if (child_error != NULL) {
					/* Error! */
					g_propagate_error (error, child_error);
					return;
				}
			}

			break;
		}
		case DFSM_AST_DATA_DICT: {
			GVariantIter iter;
			GVariant *child_entry_variant;
			GHashTable *data_structure_map;
			guint i;

			/* Loop through the data structure and copy all the dict entries into a hash table. This makes lookups faster, ensures that
			 * we're not constantly re-evaluating expressions for keys, and thus means the key values are set in stone before we start
			 * potentially modifying them by making assignments. */
			data_structure_map = g_hash_table_new_full (g_variant_hash, g_variant_equal, (GDestroyNotify) g_variant_unref,
			                                            dfsm_ast_node_unref);

			for (i = 0; i < data_structure->dict_val->len; i++) {
				DfsmAstDictionaryEntry *child_entry;
				GVariant *key_variant;
				GError *child_error = NULL;

				child_entry = (DfsmAstDictionaryEntry*) g_ptr_array_index (data_structure->dict_val, i);

				/* Evaluate the expression for the entry's key, but not its value. We'll store a pointer to the value verbatim. */
				key_variant = dfsm_ast_expression_evaluate (child_entry->key, environment, &child_error);

				if (child_error != NULL) {
					g_hash_table_unref (data_structure_map);
					g_propagate_error (error, child_error);
					return;
				}

				/* Insert the entry into the map. */
				g_hash_table_insert (data_structure_map, g_variant_ref (key_variant), dfsm_ast_node_ref (child_entry->value));

				g_variant_unref (key_variant);
			}

			/* We should only assign to the values in the data structure dict which are listed in the variant dict. i.e. We touch the
			 * values corresponding to the intersection of the keys of the data structure and variant dicts. */
			g_variant_iter_init (&iter, new_value);

			while ((child_entry_variant = g_variant_iter_next_value (&iter)) != NULL) {
				GVariant *child_key_variant, *child_value_variant;
				DfsmAstExpression *value_expression;
				DfsmAstDataStructure *value_data_structure;
				GError *child_error = NULL;

				/* Get the child variant and its key and value. */
				child_entry_variant = g_variant_get_child_value (new_value, i);
				child_key_variant = g_variant_get_child_value (child_entry_variant, 0);
				child_value_variant = g_variant_get_child_value (child_entry_variant, 1);

				g_variant_unref (child_entry_variant);

				/* Find the corresponding entry in the data structure dict, if it exists. */
				value_expression = g_hash_table_lookup (data_structure_map, child_key_variant);

				g_variant_unref (child_key_variant);

				/* TODO: For the moment, we hackily assume that the child_expression is a DFSM_AST_EXPRESSION_DATA_STRUCTURE and
				 * extract its data structure to recursively assign to. */
				g_assert (value_expression->expression_type == DFSM_AST_EXPRESSION_DATA_STRUCTURE);
				value_data_structure = ((DfsmAstExpressionDataStructure*) value_expression)->data_structure;

				/* Recursively assign to the child data structure. */
				dfsm_ast_data_structure_set_from_variant (value_data_structure, environment, child_value_variant, &child_error);

				g_variant_unref (child_value_variant);

				if (child_error != NULL) {
					/* Error! */
					g_hash_table_unref (data_structure_map);
					g_propagate_error (error, child_error);
					return;
				}
			}

			g_hash_table_unref (data_structure_map);

			break;
		}
		case DFSM_AST_DATA_VARIABLE:
			dfsm_ast_variable_set_from_variant (data_structure->variable_val, environment, new_value, error);
			break;
		default:
			g_assert_not_reached ();
	}
}

/**
 * dfsm_ast_fuzzy_data_structure_new:
 * @data_structure: (transfer full): a data structure to wrap
 * @weight: weight of the structure for fuzzing, or %NAN
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstFuzzyDataStructure wrapping (and taking ownership of) the given @data_structure. The @weight determines how “important” the
 * @data_structure is for fuzzing. %NAN indicates no preference as to the structure's weight.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstDataStructure *
dfsm_ast_fuzzy_data_structure_new (DfsmAstDataStructure *data_structure, gdouble weight, GError **error)
{
	DfsmAstFuzzyDataStructure *fuzzy_data_structure;

	g_return_val_if_fail (data_structure != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	fuzzy_data_structure = g_slice_new (DfsmAstFuzzyDataStructure);

	fuzzy_data_structure->parent = *data_structure;
	fuzzy_data_structure->weight = weight;
	fuzzy_data_structure->is_fuzzy = TRUE;

	return (DfsmAstDataStructure*) fuzzy_data_structure;
}

/**
 * dfsm_ast_data_item_new:
 * @type_string: a D-Bus–style type string for the data item
 * @value_expression: an expression giving the value of the data item
 *
 * Create a new #DfsmAstDataItem with the given type and value expression.
 *
 * Return value: (transfer full): a new data item
 */
DfsmAstDataItem *
dfsm_ast_data_item_new (const gchar *type_string, DfsmAstDataStructure *value_expression)
{
	DfsmAstDataItem *data_item;

	g_return_val_if_fail (type_string != NULL && *type_string != '\0', NULL);
	g_return_val_if_fail (value_expression != NULL, NULL);

	data_item = g_slice_new (DfsmAstDataItem);

	data_item->type_string = g_strdup (type_string);
	data_item->value_expression = dfsm_ast_node_ref (value_expression);

	return data_item;
}

/**
 * dfsm_ast_data_item_free:
 * @data_item: the data item to free
 *
 * Free the given @data_item.
 */
void
dfsm_ast_data_item_free (DfsmAstDataItem *data_item)
{
	if (data_item != NULL) {
		dfsm_ast_node_unref (data_item->value_expression);
		g_free (data_item->type_string);

		g_slice_free (DfsmAstDataItem, data_item);
	}
}

/**
 * dfsm_ast_data_item_check:
 * @data_item: a #DfsmAstDataItem
 * @error: a #GError
 *
 * Check the given @data_item and its descendents are correct; for example by type checking them.
 */
void
dfsm_ast_data_item_check (DfsmAstDataItem *data_item, GError **error)
{
	GVariantType *expected_type, *value_type;

	g_return_if_fail (data_item != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	/* Conditions which should always hold, regardless of user input. */
	g_assert (data_item->type_string != NULL);
	g_assert (data_item->value_expression != NULL);

	/* Conditions which may not hold as a result of invalid user input. */

	/* Valid signature? */
	if (g_variant_type_string_is_valid (data_item->type_string) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus type signature: %s", data_item->type_string);
		return;
	}

	/* Valid expression? */
	dfsm_ast_node_check ((DfsmAstNode*) data_item->value_expression, error);

	if (*error != NULL) {
		return;
	}

	/* Equal type? */
	expected_type = g_variant_type_new (data_item->type_string);
	value_type = dfsm_ast_data_structure_calculate_type (data_item->value_expression, NULL /* TODO: Split all *_check() functions up */);

	if (g_variant_type_equal (expected_type, value_type) == FALSE) {
		gchar *expected, *received;

		expected = g_variant_type_dup_string (expected_type);
		received = g_variant_type_dup_string (value_type);

		g_variant_type_free (value_type);
		g_variant_type_free (expected_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch between signature and value of data item: expected type %s but received type %s.",
		             expected, received);

		return;
	}

	g_variant_type_free (value_type);
	g_variant_type_free (expected_type);
}

static void
_dfsm_ast_transition_check (DfsmAstNode *node, GError **error)
{
	guint i;
	DfsmAstTransition *transition;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	transition = (DfsmAstTransition*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (transition->parent.node_type == DFSM_AST_NODE_TRANSITION);
	g_assert (transition->from_state_name != NULL);
	g_assert (transition->to_state_name != NULL);
	g_assert (transition->preconditions != NULL);
	g_assert (transition->statements != NULL);

	switch (transition->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			g_assert (transition->trigger_params.method_name != NULL);
			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	for (i = 0; i < transition->preconditions->len; i++) {
		g_assert (g_ptr_array_index (transition->preconditions, i) != NULL);
	}

	for (i = 0; i < transition->statements->len; i++) {
		g_assert (g_ptr_array_index (transition->statements, i) != NULL);
	}

	/* Conditions which may not hold as a result of invalid user input. */
	if (dfsm_is_state_name (transition->from_state_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid state name: %s", transition->from_state_name);
		return;
	} else if (dfsm_is_state_name (transition->to_state_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid state name: %s", transition->to_state_name);
		return;
	}

	/* TODO: Check two state names exist */

	switch (transition->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			if (g_dbus_is_member_name (transition->trigger_params.method_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus method name: %s",
				             transition->trigger_params.method_name);
				return;
			}

			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	for (i = 0; i < transition->preconditions->len; i++) {
		DfsmAstPrecondition *precondition;

		precondition = (DfsmAstPrecondition*) g_ptr_array_index (transition->preconditions, i);

		dfsm_ast_node_check ((DfsmAstNode*) precondition, error);

		if (*error != NULL) {
			return;
		}
	}

	for (i = 0; i < transition->statements->len; i++) {
		DfsmAstStatement *statement;

		statement = (DfsmAstStatement*) g_ptr_array_index (transition->statements, i);

		dfsm_ast_node_check ((DfsmAstNode*) statement, error);

		if (*error != NULL) {
			return;
		}
	}
}

static void
_dfsm_ast_transition_free (DfsmAstNode *node)
{
	DfsmAstTransition *transition = (DfsmAstTransition*) node;

	if (transition != NULL) {
		g_ptr_array_unref (transition->statements);
		g_ptr_array_unref (transition->preconditions);

		switch (transition->trigger) {
			case DFSM_AST_TRANSITION_METHOD_CALL:
				g_free (transition->trigger_params.method_name);
				break;
			case DFSM_AST_TRANSITION_ARBITRARY:
				/* Nothing to free here */
				break;
			default:
				g_assert_not_reached ();
		}

		g_free (transition->to_state_name);
		g_free (transition->from_state_name);

		g_slice_free (DfsmAstTransition, transition);
	}
}

/**
 * dfsm_ast_transition_new:
 * @from_state_name: name of the FSM state being transitioned out of
 * @to_state_name: name of the FSM state being transitioned into
 * @transition_type: method name of the transition trigger, or ‘*’ for an arbitrary transition
 * @precondition: array of #DfsmAstPrecondition<!-- -->s for the transition
 * @statements: array of #DfsmAstStatement<!-- -->s to execute with the transition
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstTransition representing a single transition from @from_state_name to @to_state_name.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstTransition *
dfsm_ast_transition_new (const gchar *from_state_name, const gchar *to_state_name, const gchar *transition_type,
                         GPtrArray/*<DfsmAstPrecondition>*/ *preconditions, GPtrArray/*<DfsmAstStatement>*/ *statements, GError **error)
{
	DfsmAstTransition *transition;

	g_return_val_if_fail (from_state_name != NULL && *from_state_name != '\0', NULL);
	g_return_val_if_fail (to_state_name != NULL && *to_state_name != '\0', NULL);
	g_return_val_if_fail (transition_type != NULL && *transition_type != '\0', NULL);
	g_return_val_if_fail (preconditions != NULL, NULL);
	g_return_val_if_fail (statements != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	transition = g_slice_new (DfsmAstTransition);

	_dfsm_ast_node_init (&(transition->parent), DFSM_AST_NODE_TRANSITION, _dfsm_ast_transition_check, _dfsm_ast_transition_free);

	transition->from_state_name = g_strdup (from_state_name);
	transition->to_state_name = g_strdup (to_state_name);

	if (strcmp (transition_type, "*") == 0) {
		transition->trigger = DFSM_AST_TRANSITION_ARBITRARY;
	} else {
		transition->trigger = DFSM_AST_TRANSITION_METHOD_CALL;
		transition->trigger_params.method_name = g_strdup (transition_type);
	}

	transition->preconditions = g_ptr_array_ref (preconditions);
	transition->statements = g_ptr_array_ref (statements);

	return transition;
}

/**
 * dfsm_ast_transition_check_preconditions:
 * @transition: a #DfsmAstTransition
 * @environment: the environment to execute the transition in
 * @error: a #GError
 *
 * Check the preconditions of the given @transition in the state given by @environment. The @environment will not be modified.
 *
 * If the preconditions are satisfied, %TRUE will be returned; %FALSE will be returned otherwise. If the preconditions are not satisfied and they
 * specified a D-Bus error to be thrown on failure, the error will be set in @error and %FALSE will be returned.
 *
 * Return value: %TRUE if the transition's preconditions are satisfied; %FALSE otherwise
 */
gboolean
dfsm_ast_transition_check_preconditions (DfsmAstTransition *transition, DfsmEnvironment *environment, GError **error)
{
	guint i;

	g_return_val_if_fail (transition != NULL, FALSE);
	g_return_val_if_fail (environment != NULL, FALSE);
	g_return_val_if_fail (error != NULL && *error == NULL, FALSE);

	/* Check each of the preconditions in order and return when the first one fails. */
	for (i = 0; i < transition->preconditions->len; i++) {
		DfsmAstPrecondition *precondition = (DfsmAstPrecondition*) g_ptr_array_index (transition->preconditions, i);

		if (dfsm_ast_precondition_check (precondition, environment, error) == FALSE) {
			return FALSE;
		}
	}

	g_assert (*error == NULL);

	return TRUE;
}

/**
 * dfsm_ast_transition_execute:
 * @transition: a #DfsmAstTransition
 * @environment: the environment to execute the transition in
 * @error: a #GError
 *
 * Execute a given state machine transition. This may modify the @environment. It assumes that dfsm_ast_transition_check_preconditions() has already
 * been called for this @transition and @environment and has returned %TRUE. It is an error to call this function otherwise.
 *
 * If the transition is successful (i.e. a D-Bus reply is the result), the parameters of the reply will be returned. If the transition is unsuccessful
 * (i.e. a D-Bus error is thrown) the error will be returned in @error.
 *
 * Return value: (transfer full): reply parameters from the transition, or %NULL
 */
GVariant *
dfsm_ast_transition_execute (DfsmAstTransition *transition, DfsmEnvironment *environment, GError **error)
{
	GVariant *return_value = NULL;
	guint i;

	g_return_val_if_fail (transition != NULL, NULL);
	g_return_val_if_fail (environment != NULL, NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);

	for (i = 0; i < transition->statements->len; i++) {
		DfsmAstStatement *statement;
		GVariant *_return_value;
		GError *child_error = NULL;

		statement = g_ptr_array_index (transition->statements, i);
		_return_value = dfsm_ast_statement_execute (statement, environment, &child_error);

		g_assert (_return_value == NULL || return_value == NULL);
		g_assert (_return_value == NULL || child_error == NULL);

		if (_return_value != NULL) {
			return_value = _return_value;
		} else if (child_error != NULL) {
			g_propagate_error (error, child_error);
			return NULL;
		}
	}

	return return_value;
}

static void
_dfsm_ast_precondition_check (DfsmAstNode *node, GError **error)
{
	DfsmAstPrecondition *precondition;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	precondition = (DfsmAstPrecondition*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (precondition->parent.node_type == DFSM_AST_NODE_PRECONDITION);
	g_assert (precondition->condition != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	if (precondition->error_name != NULL && g_dbus_is_member_name (precondition->error_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus error name: %s", precondition->error_name);
		return;
	}

	dfsm_ast_node_check ((DfsmAstNode*) precondition->condition, error);

	if (*error != NULL) {
		return;
	}

	/* TODO: Assert that the precondition is a boolean */
}

static void
_dfsm_ast_precondition_free (DfsmAstNode *node)
{
	DfsmAstPrecondition *precondition = (DfsmAstPrecondition*) node;

	if (precondition != NULL) {
		dfsm_ast_node_unref (precondition->condition);
		g_free (precondition->error_name);

		g_slice_free (DfsmAstPrecondition, precondition);
	}
}

/**
 * dfsm_ast_precondition_new:
 * @error_name: (allow-none): name of the D-Bus error to throw on precondition failure, or %NULL
 * @condition: the condition to fulfil for the precondition
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstPrecondition for the given @condition.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstPrecondition *
dfsm_ast_precondition_new (const gchar *error_name /* nullable */, DfsmAstExpression *condition, GError **error)
{
	DfsmAstPrecondition *precondition;

	g_return_val_if_fail (error_name == NULL || *error_name != '\0', NULL);
	g_return_val_if_fail (condition != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	precondition = g_slice_new (DfsmAstPrecondition);

	_dfsm_ast_node_init (&(precondition->parent), DFSM_AST_NODE_PRECONDITION, _dfsm_ast_precondition_check, _dfsm_ast_precondition_free);

	precondition->error_name = g_strdup (error_name);
	precondition->condition = dfsm_ast_node_ref (condition);

	return precondition;
}

/**
 * dfsm_ast_precondition_check:
 * @precondition: a #DfsmAstPrecondition
 * @environment: a #DfsmEnvironment containing all variables
 * @error: a #GError
 *
 * TODO
 *
 * Return value: TODO
 */
gboolean
dfsm_ast_precondition_check (DfsmAstPrecondition *precondition, DfsmEnvironment *environment, GError **error)
{
	GVariant *condition_value;
	gboolean condition_holds;
	GError *child_error = NULL;

	g_return_val_if_fail (precondition != NULL, FALSE);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), FALSE);
	g_return_val_if_fail (error != NULL && *error == NULL, FALSE);

	/* Evaluate the condition. */
	condition_value = dfsm_ast_expression_evaluate (precondition->condition, environment, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return FALSE;
	}

	/* If the condition doesn't hold and we have an error, throw that. */
	condition_holds = g_variant_get_boolean (condition_value);

	if (condition_holds == FALSE && precondition->error_name != NULL) {
		g_dbus_error_set_dbus_error (error, precondition->error_name, "Precondition failed.", NULL);
	}

	return condition_holds;
}

/*
 * _dfsm_ast_statement_init:
 * @statement: a statement to initialise
 * @statement_type: type of the statement
 * @check_func: a function to check the statement and its descendents
 * @free_func: a function to free the statement
 * @execute_func: a function to execute the statement
 *
 * Initialise the given statement.
 */
static void
_dfsm_ast_statement_init (DfsmAstStatement *statement, DfsmAstStatementType statement_type, DfsmAstNodeCheckFunc check_func,
                          DfsmAstNodeFreeFunc free_func, DfsmAstStatementExecuteFunc execute_func)
{
	g_return_if_fail (statement != NULL);

	_dfsm_ast_node_init (&(statement->parent), DFSM_AST_NODE_STATEMENT, check_func, free_func);

	statement->statement_type = statement_type;
	statement->execute_func = execute_func;
}

/**
 * dfsm_ast_statement_execute:
 * @statement: a #DfsmAstStatement
 * @environment: the environment to execute the statement in
 * @error: a #GError
 *
 * Execute a given state machine statement. This may modify the @environment.
 *
 * If the statement is successful (i.e. a D-Bus reply is the result), the parameters of the reply will be returned. If the statement is unsuccessful
 * (i.e. a D-Bus error is thrown) the error will be returned in @error.
 *
 * Return value: (transfer full): reply parameters from the statement, or %NULL
 */
GVariant *
dfsm_ast_statement_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	g_return_val_if_fail (statement != NULL, NULL);
	g_return_val_if_fail (environment != NULL, NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);

	g_assert (statement->execute_func != NULL);
	return statement->execute_func (statement, environment, error);
}

static void
_dfsm_ast_statement_assignment_check (DfsmAstNode *node, GError **error)
{
	GVariantType *lvalue_type, *rvalue_type;
	DfsmAstStatementAssignment *assignment;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	assignment = (DfsmAstStatementAssignment*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (assignment->parent.parent.node_type == DFSM_AST_NODE_STATEMENT);
	g_assert (assignment->parent.statement_type == DFSM_AST_STATEMENT_ASSIGNMENT);
	g_assert (assignment->data_structure != NULL);
	g_assert (assignment->expression != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	dfsm_ast_node_check ((DfsmAstNode*) assignment->data_structure, error);

	if (*error != NULL) {
		return;
	}

	dfsm_ast_node_check ((DfsmAstNode*) assignment->expression, error);

	if (*error != NULL) {
		return;
	}

	lvalue_type = dfsm_ast_data_structure_calculate_type (assignment->data_structure, NULL /* TODO */);
	rvalue_type = dfsm_ast_expression_calculate_type (assignment->expression);

	if (g_variant_type_is_subtype_of (rvalue_type, lvalue_type) == FALSE) {
		gchar *expected, *received;

		expected = g_variant_type_dup_string (lvalue_type);
		received = g_variant_type_dup_string (rvalue_type);

		g_variant_type_free (lvalue_type);
		g_variant_type_free (rvalue_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             "Type mismatch for assignment: expected l-value type %s but received r-value type %s.", expected, received);

		return;
	}

	g_variant_type_free (lvalue_type);
	g_variant_type_free (rvalue_type);
}

static void
_dfsm_ast_statement_assignment_free (DfsmAstNode *node)
{
	DfsmAstStatementAssignment *assignment = (DfsmAstStatementAssignment*) node;

	if (assignment != NULL) {
		dfsm_ast_node_unref (assignment->expression);
		dfsm_ast_node_unref (assignment->data_structure);

		g_slice_free (DfsmAstStatementAssignment, assignment);
	}
}

static GVariant *
_dfsm_ast_statement_assignment_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	GVariant *rvalue;
	GError *child_error = NULL;
	DfsmAstStatementAssignment *assignment = (DfsmAstStatementAssignment*) statement;

	/* Evaluate the rvalue */
	rvalue = dfsm_ast_expression_evaluate (assignment->expression, environment, &child_error);
	g_assert ((rvalue == NULL) != (child_error == NULL));

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Perform the assignment */
	dfsm_ast_data_structure_set_from_variant (assignment->data_structure, environment, rvalue, &child_error);

	g_variant_unref (rvalue);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	return NULL;
}

/**
 * dfsm_ast_statement_assignment_new:
 * @data_structure: data structure being assigned to (lvalue)
 * @expression: expression to be evaluated for assignment (rvalue)
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstStatement for an assignment of @expression to @data_structure.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_assignment_new (DfsmAstDataStructure *data_structure, DfsmAstExpression *expression, GError **error)
{
	DfsmAstStatementAssignment *statement;

	g_return_val_if_fail (data_structure != NULL, NULL);
	g_return_val_if_fail (expression != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	statement = g_slice_new (DfsmAstStatementAssignment);

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_ASSIGNMENT, _dfsm_ast_statement_assignment_check,
	                          _dfsm_ast_statement_assignment_free, _dfsm_ast_statement_assignment_execute);

	statement->data_structure = dfsm_ast_node_ref (data_structure);
	statement->expression = dfsm_ast_node_ref (expression);

	return (DfsmAstStatement*) statement;
}

static void
_dfsm_ast_statement_throw_check (DfsmAstNode *node, GError **error)
{
	DfsmAstStatementThrow *throw;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	throw = (DfsmAstStatementThrow*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (throw->parent.parent.node_type == DFSM_AST_NODE_STATEMENT);
	g_assert (throw->parent.statement_type == DFSM_AST_STATEMENT_THROW);
	g_assert (throw->error_name != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	if (g_dbus_is_member_name (throw->error_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus error name: %s", throw->error_name);
		return;
	}

	/* TODO: Check we're actually allowed to throw. */
}

static void
_dfsm_ast_statement_throw_free (DfsmAstNode *node)
{
	DfsmAstStatementThrow *throw = (DfsmAstStatementThrow*) node;

	if (throw != NULL) {
		g_free (throw->error_name);

		g_slice_free (DfsmAstStatementThrow, throw);
	}
}

static GVariant *
_dfsm_ast_statement_throw_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	gchar *message;
	DfsmAstStatementThrow *throw = (DfsmAstStatementThrow*) statement;

	message = g_strdup_printf ("Error message generated by %s().", G_STRFUNC);
	g_dbus_error_set_dbus_error (error, throw->error_name, message, NULL);
	g_free (message);

	return NULL;
}

/**
 * dfsm_ast_statement_throw_new:
 * @error_name: name of the D-Bus error to throw
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstStatement for throwing a D-Bus error of type @error_name.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_throw_new (const gchar *error_name, GError **error)
{
	DfsmAstStatementThrow *statement;

	g_return_val_if_fail (error_name != NULL && *error_name != '\0', NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	statement = g_slice_new (DfsmAstStatementThrow);

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_THROW, _dfsm_ast_statement_throw_check, _dfsm_ast_statement_throw_free,
	                          _dfsm_ast_statement_throw_execute);

	statement->error_name = g_strdup (error_name);

	return (DfsmAstStatement*) statement;
}

static void
_dfsm_ast_statement_emit_check (DfsmAstNode *node, GError **error)
{
	DfsmAstStatementEmit *emit;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	emit = (DfsmAstStatementEmit*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (emit->parent.parent.node_type == DFSM_AST_NODE_STATEMENT);
	g_assert (emit->parent.statement_type == DFSM_AST_STATEMENT_EMIT);
	g_assert (emit->signal_name != NULL);
	g_assert (emit->expression != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	if (g_dbus_is_member_name (emit->signal_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus signal name: %s", emit->signal_name);
		return;
	}

	dfsm_ast_node_check ((DfsmAstNode*) emit->expression, error);

	if (*error != NULL) {
		return;
	}

	/* TODO: Check expression's type matches the signal. Check the signal's from the right interface. */
}

static void
_dfsm_ast_statement_emit_free (DfsmAstNode *node)
{
	DfsmAstStatementEmit *emit = (DfsmAstStatementEmit*) node;

	if (emit != NULL) {
		dfsm_ast_node_unref (emit->expression);
		g_free (emit->signal_name);

		g_slice_free (DfsmAstStatementEmit, emit);
	}
}

static GVariant *
_dfsm_ast_statement_emit_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	GVariant *expression_value;
	DfsmAstStatementEmit *emit = (DfsmAstStatementEmit*) statement;
	GError *child_error = NULL;

	/* Evaluate the child expression first. */
	expression_value = dfsm_ast_expression_evaluate (emit->expression, environment, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Emit the signal. */
	dfsm_environment_emit_signal (environment, emit->signal_name, expression_value, &child_error);

	g_variant_unref (expression_value);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	return NULL;
}

/**
 * dfsm_ast_statement_emit_new:
 * @signal_name: name of the D-Bus signal to emit
 * @expression: expression to evaluate as the signal parameters
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstStatement for emitting a D-Bus signal of name @signal_name with value given by @expression.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_emit_new (const gchar *signal_name, DfsmAstExpression *expression, GError **error)
{
	DfsmAstStatementEmit *statement;

	g_return_val_if_fail (signal_name != NULL && *signal_name != '\0', NULL);
	g_return_val_if_fail (expression != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	statement = g_slice_new (DfsmAstStatementEmit);

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_EMIT, _dfsm_ast_statement_emit_check, _dfsm_ast_statement_emit_free,
	                          _dfsm_ast_statement_emit_execute);

	statement->signal_name = g_strdup (signal_name);
	statement->expression = dfsm_ast_node_ref (expression);

	return (DfsmAstStatement*) statement;
}

static void
_dfsm_ast_statement_reply_check (DfsmAstNode *node, GError **error)
{
	DfsmAstStatementReply *reply;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	reply = (DfsmAstStatementReply*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (reply->parent.parent.node_type == DFSM_AST_NODE_STATEMENT);
	g_assert (reply->parent.statement_type == DFSM_AST_STATEMENT_REPLY);
	g_assert (reply->expression != NULL);

	/* Conditions which may not hold as a result of invalid user input. */
	dfsm_ast_node_check ((DfsmAstNode*) reply->expression, error);

	if (*error != NULL) {
		return;
	}

	/* TODO: Check expression's type matches the method. Check we're actually allowed to reply. */
}

static void
_dfsm_ast_statement_reply_free (DfsmAstNode *node)
{
	DfsmAstStatementReply *reply = (DfsmAstStatementReply*) node;

	if (reply != NULL) {
		dfsm_ast_node_unref (reply->expression);

		g_slice_free (DfsmAstStatementReply, reply);
	}
}

static GVariant *
_dfsm_ast_statement_reply_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	GVariant *value;
	GError *child_error = NULL;
	DfsmAstStatementReply *reply = (DfsmAstStatementReply*) statement;

	/* Evaluate the expression */
	value = dfsm_ast_expression_evaluate (reply->expression, environment, &child_error);
	g_assert ((value == NULL) != (child_error == NULL));

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	return value;
}

/**
 * dfsm_ast_statement_reply_new:
 * @expression: expression to evaluate as the reply value
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstStatement for replying to the triggering D-Bus method call with value given by @expression.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_reply_new (DfsmAstExpression *expression, GError **error)
{
	DfsmAstStatementReply *statement;

	g_return_val_if_fail (expression != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	statement = g_slice_new (DfsmAstStatementReply);

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_REPLY, _dfsm_ast_statement_reply_check, _dfsm_ast_statement_reply_free,
	                          _dfsm_ast_statement_reply_execute);

	statement->expression = dfsm_ast_node_ref (expression);

	return (DfsmAstStatement*) statement;
}

static void
_dfsm_ast_variable_check (DfsmAstNode *node, GError **error)
{
	DfsmAstVariable *variable;

	g_return_if_fail (node != NULL);
	g_return_if_fail (error != NULL && *error == NULL);

	variable = (DfsmAstVariable*) node;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (variable->parent.node_type == DFSM_AST_NODE_VARIABLE);
	g_assert (variable->variable_name != NULL);

	switch (variable->scope) {
		case DFSM_VARIABLE_SCOPE_LOCAL:
		case DFSM_VARIABLE_SCOPE_OBJECT:
			/* Nothing to do */
			break;
		default:
			g_assert_not_reached ();
	}

	/* Conditions which may not hold as a result of invalid user input. */
	if (dfsm_is_variable_name (variable->variable_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid variable name: %s", variable->variable_name);
		return;
	}

	/* TODO: Check variable exists, is in scope, etc. */
}

static void
_dfsm_ast_variable_free (DfsmAstNode *node)
{
	DfsmAstVariable *variable = (DfsmAstVariable*) node;

	if (variable != NULL) {
		g_free (variable->variable_name);

		g_slice_free (DfsmAstVariable, variable);
	}
}

/**
 * dfsm_ast_variable_new:
 * @scope: scope of the variable reference
 * @variable_name: name of the variable being referenced
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstVariable representing a variable dereferencing operation.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstVariable *
dfsm_ast_variable_new (DfsmVariableScope scope, const gchar *variable_name, GError **error)
{
	DfsmAstVariable *variable;

	g_return_val_if_fail (variable_name != NULL && *variable_name != '\0', NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	variable = g_slice_new (DfsmAstVariable);

	_dfsm_ast_node_init (&(variable->parent), DFSM_AST_NODE_VARIABLE, _dfsm_ast_variable_check, _dfsm_ast_variable_free);

	variable->scope = scope;
	variable->variable_name = g_strdup (variable_name);

	return variable;
}

/**
 * dfsm_ast_variable_calculate_type:
 * @variable: a #DfsmAstVariable
 * @environment: a #DfsmEnvironment containing all variables
 *
 * Calculate the type of the given @variable.
 *
 * This assumes that the variable has already been checked, and so this does not perform any type checking of its own.
 *
 * Return value: (transfer full): the type of the variable
 */
GVariantType *
dfsm_ast_variable_calculate_type (DfsmAstVariable *variable, DfsmEnvironment *environment)
{
	g_return_val_if_fail (variable != NULL, NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	return dfsm_environment_dup_variable_type (environment, variable->scope, variable->variable_name);
}

/**
 * dfsm_ast_variable_to_variable:
 * @variable: a #DfsmAstVariable
 * @environment: a #DfsmEnvironment containing all variables
 * @error: (allow-none): a #GError, or %NULL
 *
 * Convert the value stored by @variable to a #GVariant in the given @environment.
 *
 * This assumes that the @variable has been successfully checked by dfsm_ast_node_check() beforehand. It is an error to call this function otherwise.
 *
 * Return value: (transfer full): the #GVariant version of the @variable's value
 */
GVariant *
dfsm_ast_variable_to_variant (DfsmAstVariable *variable, DfsmEnvironment *environment, GError **error)
{
	g_return_val_if_fail (variable != NULL, NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	return dfsm_environment_dup_variable_value (environment, variable->scope, variable->variable_name);
}

/**
 * dfsm_ast_variable_set_from_variant:
 * @variable: a #DfsmAstVariable
 * @environment: a #DfsmEnvironment containing all variables
 * @new_value: the new value to assign to the variable
 * @error: (allow-none): a #GError, or %NULL
 *
 * Set the given @variable's value in @environment to the #GVariant value given in @new_value. This will overwrite any existing value stored for
 * the given variable, and won't recursively assign to structures inside the variable's existing value.
 */
void
dfsm_ast_variable_set_from_variant (DfsmAstVariable *variable, DfsmEnvironment *environment, GVariant *new_value, GError **error)
{
	g_return_if_fail (variable != NULL);
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (new_value != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	dfsm_environment_set_variable_value (environment, variable->scope, variable->variable_name, new_value);
}
