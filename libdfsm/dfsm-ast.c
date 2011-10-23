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

#include "dfsm-ast.h"
#include "dfsm-utils.h"

/*
 * _dfsm_ast_node_init:
 * @node: a #DfsmAstNode
 * @node_type: the type of the node
 * @free_func: a function to free the node
 *
 * Initialise the members of the given @node.
 */
static void
_dfsm_ast_node_init (DfsmAstNode *node, DfsmAstNodeType node_type, void (*free_func) (DfsmAstNode *node))
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (free_func != NULL);

	node->node_type = node_type;
	node->free_func = free_func;
	node->ref_count = 1;
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

	_dfsm_ast_node_init (&(object->parent), DFSM_AST_NODE_OBJECT, _dfsm_ast_object_free);

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
				/* TODO: Error */
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
					/* TODO: Error */
				}
			}

			g_ptr_array_add (object->states, g_strdup (state_name));
		}
	}

	/* TODO: Error if there are no states, etc. */

	return object;
}

/*
 * _dfsm_ast_expression_init:
 * @expression: a #DfsmAstExpression
 * @node_type: the type of the expression
 * @free_func: a function to free the expression
 *
 * Initialise the members of the given @expression.
 */
static void
_dfsm_ast_expression_init (DfsmAstExpression *expression, DfsmAstExpressionType expression_type, void (*free_func) (DfsmAstNode *node))
{
	g_return_if_fail (expression != NULL);

	_dfsm_ast_node_init (&(expression->parent), DFSM_AST_NODE_EXPRESSION, free_func);

	expression->expression_type = expression_type;
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

	_dfsm_ast_expression_init (&(expression->parent), DFSM_AST_EXPRESSION_FUNCTION_CALL, _dfsm_ast_expression_function_call_free);

	expression->function_name = g_strdup (function_name);
	expression->parameters = dfsm_ast_node_ref (parameters);

	/* TODO: Error on invalid function name or parameters. */

	return (DfsmAstExpression*) expression;
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

	_dfsm_ast_expression_init (&(expression->parent), DFSM_AST_EXPRESSION_DATA_STRUCTURE, _dfsm_ast_expression_data_structure_free);

	expression->data_structure = dfsm_ast_node_ref (data_structure);

	return (DfsmAstExpression*) expression;
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
		/* Function calls */
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
			g_assert_not_reached ();
		/* Unary expressions */
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
			/* TODO: Any special checks required by these types. */
			break;
		/* Binary expressions */
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
			g_assert_not_reached ();
		default:
			/* TODO: Error */
			g_assert_not_reached ();
	}

	expression = g_slice_new (DfsmAstExpressionUnary);

	_dfsm_ast_expression_init (&(expression->parent), expression_type, _dfsm_ast_expression_unary_free);

	expression->child_node = dfsm_ast_node_ref (child_node);

	return (DfsmAstExpression*) expression;
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
		/* Function calls */
		case DFSM_AST_EXPRESSION_FUNCTION_CALL:
			g_assert_not_reached ();
		/* Unary expressions */
		case DFSM_AST_EXPRESSION_DATA_STRUCTURE:
		case DFSM_AST_EXPRESSION_NOT:
			g_assert_not_reached ();
		/* Binary expressions */
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
			/* TODO: Any special checks required by these types. */
			break;
		default:
			/* TODO: Error */
			g_assert_not_reached ();
	}

	expression = g_slice_new (DfsmAstExpressionBinary);

	_dfsm_ast_expression_init (&(expression->parent), expression_type, _dfsm_ast_expression_binary_free);

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
_dfsm_ast_data_structure_free (DfsmAstNode *node)
{
	DfsmAstDataStructure *data_structure = (DfsmAstDataStructure*) node;

	if (data_structure != NULL) {
		switch (data_structure->data_structure_type) {
			case DFSM_AST_DATA_STRING:
				g_free (data_structure->str);
				break;
			case DFSM_AST_DATA_INTEGER:
			case DFSM_AST_DATA_FLOAT:
			case DFSM_AST_DATA_BOOLEAN:
				/* Nothing to free here */
				break;
			case DFSM_AST_DATA_REGEXP:
				g_free (data_structure->regexp);
				break;
			case DFSM_AST_DATA_ARRAY:
				g_ptr_array_unref (data_structure->array);
				break;
			case DFSM_AST_DATA_DICTIONARY:
				g_ptr_array_unref (data_structure->dictionary);
				break;
			case DFSM_AST_DATA_VARIABLE:
				dfsm_ast_node_unref (data_structure->variable);
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

	_dfsm_ast_node_init (&(data_structure->parent), DFSM_AST_NODE_DATA_STRUCTURE, _dfsm_ast_data_structure_free);

	switch (data_structure_type) {
		/* TODO: Per-type validation */
		case DFSM_AST_DATA_STRING:
			data_structure->str = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_INTEGER:
			data_structure->integer = *((gint64*) value);
			break;
		case DFSM_AST_DATA_FLOAT:
			data_structure->flt = *((gdouble*) value);
			break;
		case DFSM_AST_DATA_BOOLEAN:
			data_structure->boolean = (GPOINTER_TO_UINT (value) == 1) ? TRUE : FALSE;
			break;
		case DFSM_AST_DATA_REGEXP:
			data_structure->regexp = g_strdup ((gchar*) value);
			break;
		case DFSM_AST_DATA_ARRAY:
			data_structure->array = g_ptr_array_ref (value); /* array of DfsmAstExpressions */
			break;
		case DFSM_AST_DATA_DICTIONARY:
			data_structure->dictionary = g_ptr_array_ref (value); /* array of DfsmAstDictionaryEntrys */
			break;
		case DFSM_AST_DATA_VARIABLE:
			data_structure->variable = dfsm_ast_node_ref (value); /* DfsmAstVariable */
			break;
		default:
			g_assert_not_reached ();
	}

	data_structure->data_structure_type = data_structure_type;

	return data_structure;
}

/**
 * dfsm_ast_fuzzy_data_structure_new:
 * @data_structure: (transfer full): a data structure to wrap
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstFuzzyDataStructure wrapping (and taking ownership of) the given @data_structure.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstDataStructure *
dfsm_ast_fuzzy_data_structure_new (DfsmAstDataStructure *data_structure, GError **error)
{
	DfsmAstFuzzyDataStructure *fuzzy_data_structure;

	g_return_val_if_fail (data_structure != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	fuzzy_data_structure = g_slice_new (DfsmAstFuzzyDataStructure);

	fuzzy_data_structure->parent = *data_structure;
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

	_dfsm_ast_node_init (&(transition->parent), DFSM_AST_NODE_TRANSITION, _dfsm_ast_transition_free);

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

	_dfsm_ast_node_init (&(precondition->parent), DFSM_AST_NODE_PRECONDITION, _dfsm_ast_precondition_free);

	precondition->error_name = g_strdup (error_name);
	precondition->condition = dfsm_ast_node_ref (condition);

	/* TODO: Validate error name, expression, etc. */

	return precondition;
}

/*
 * _dfsm_ast_statement_init:
 * @statement: a statement to initialise
 * @statement_type: type of the statement
 * @free_func: a function to free the statement
 *
 * Initialise the given statement.
 */
static void
_dfsm_ast_statement_init (DfsmAstStatement *statement, DfsmAstStatementType statement_type, void (*free_func) (DfsmAstNode *node))
{
	g_return_if_fail (statement != NULL);

	_dfsm_ast_node_init (&(statement->parent), DFSM_AST_NODE_STATEMENT, free_func);

	statement->statement_type = statement_type;
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

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_ASSIGNMENT, _dfsm_ast_statement_assignment_free);

	statement->data_structure = dfsm_ast_node_ref (data_structure);
	statement->expression = dfsm_ast_node_ref (expression);

	return (DfsmAstStatement*) statement;
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

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_THROW, _dfsm_ast_statement_throw_free);

	statement->error_name = g_strdup (error_name);

	return (DfsmAstStatement*) statement;
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

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_EMIT, _dfsm_ast_statement_emit_free);

	statement->signal_name = g_strdup (signal_name);
	statement->expression = dfsm_ast_node_ref (expression);

	return (DfsmAstStatement*) statement;
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

	_dfsm_ast_statement_init (&(statement->parent), DFSM_AST_STATEMENT_REPLY, _dfsm_ast_statement_reply_free);

	statement->expression = dfsm_ast_node_ref (expression);

	return (DfsmAstStatement*) statement;
}

static void
_dfsm_ast_variable_free (DfsmAstNode *node)
{
	DfsmAstVariable *variable = (DfsmAstVariable*) node;

	if (variable != NULL) {
		g_ptr_array_unref (variable->index_expressions);
		g_free (variable->variable_name);

		g_slice_free (DfsmAstVariable, variable);
	}
}

/**
 * dfsm_ast_variable_new:
 * @scope: scope of the variable reference
 * @variable_name: name of the variable being referenced
 * @index_expressions: array of zero or more #DfsmAstExpression<!-- -->s giving the array indices used in the variable reference
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstVariable representing a variable dereferencing operation. The expressions in @index_expressions will be evaluated to give
 * the indices into an array, or keys into a dictionary, to use. Index 0 in @index_expressions gives the left-most array index (i.e. for the top-level
 * array or dictionay), which will be evaluated first.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstVariable *
dfsm_ast_variable_new (DfsmAstScope scope, const gchar *variable_name, GPtrArray/*<DfsmAstExpression>*/ *index_expressions, GError **error)
{
	DfsmAstVariable *variable;

	g_return_val_if_fail (variable_name != NULL && *variable_name != '\0', NULL);
	g_return_val_if_fail (index_expressions != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	variable = g_slice_new (DfsmAstVariable);

	_dfsm_ast_node_init (&(variable->parent), DFSM_AST_NODE_VARIABLE, _dfsm_ast_variable_free);

	variable->scope = scope;
	variable->variable_name = g_strdup (variable_name);
	variable->index_expressions = g_ptr_array_ref (index_expressions);

	return variable;
}
