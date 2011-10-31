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

#include "dfsm-utils.h"

#ifndef DFSM_AST_H
#define DFSM_AST_H

G_BEGIN_DECLS

typedef enum {
	DFSM_AST_NODE_OBJECT,
	DFSM_AST_NODE_EXPRESSION,
	DFSM_AST_NODE_DATA_STRUCTURE,
	DFSM_AST_NODE_TRANSITION,
	DFSM_AST_NODE_PRECONDITION,
	DFSM_AST_NODE_STATEMENT,
	DFSM_AST_NODE_VARIABLE,
} DfsmAstNodeType;

typedef struct _DfsmAstNode DfsmAstNode;

struct _DfsmAstNode {
	DfsmAstNodeType node_type;
	void (*free_func) (DfsmAstNode *node);
	gint ref_count;
};

gpointer dfsm_ast_node_ref (gpointer/*<DfsmAstNode>*/ node);
void dfsm_ast_node_unref (gpointer/*<DfsmAstNode>*/ node);

typedef struct {
	DfsmAstNode parent;
	gchar *object_path;
	GPtrArray *interface_names; /* array of strings */
	GHashTable *data_items; /* string for variable name → DfsmAstDataItem */
	GPtrArray *states; /* array of strings (indexed by DfsmAstStateNumber) */
	GPtrArray *transitions; /* array of DfsmAstTransitions */
} DfsmAstObject;

DfsmAstObject *dfsm_ast_object_new (const gchar *object_path, GPtrArray/*<string>*/ *interface_names, GPtrArray/*<GHashTable>*/ *data_blocks,
                                    GPtrArray/*<GPtrArray>*/ *state_blocks, GPtrArray/*<DfsmAstTransition>*/ *transition_blocks,
                                    GError **error) DFSM_CONSTRUCTOR;

typedef enum {
	/* Function calls */
	DFSM_AST_EXPRESSION_FUNCTION_CALL,

	/* Unary expressions */
	DFSM_AST_EXPRESSION_DATA_STRUCTURE,
	DFSM_AST_EXPRESSION_NOT,

	/* Binary expressions */
	DFSM_AST_EXPRESSION_TIMES,
	DFSM_AST_EXPRESSION_DIVIDE,
	DFSM_AST_EXPRESSION_MODULUS,
	DFSM_AST_EXPRESSION_PLUS,
	DFSM_AST_EXPRESSION_MINUS,
	DFSM_AST_EXPRESSION_LT,
	DFSM_AST_EXPRESSION_LTE,
	DFSM_AST_EXPRESSION_GT,
	DFSM_AST_EXPRESSION_GTE,
	DFSM_AST_EXPRESSION_EQ,
	DFSM_AST_EXPRESSION_NEQ,
	DFSM_AST_EXPRESSION_AND,
	DFSM_AST_EXPRESSION_OR,
} DfsmAstExpressionType;

/* abstract */
typedef struct {
	DfsmAstNode parent;
	DfsmAstExpressionType expression_type;
} DfsmAstExpression;

typedef struct {
	DfsmAstExpression parent;
	gchar *function_name;
	DfsmAstExpression *parameters;
} DfsmAstExpressionFunctionCall;

DfsmAstExpression *dfsm_ast_expression_function_call_new (const gchar *function_name, DfsmAstExpression *parameters, GError **error) DFSM_CONSTRUCTOR;

typedef struct _DfsmAstDataStructure DfsmAstDataStructure;

typedef struct {
	DfsmAstExpression parent;
	DfsmAstDataStructure *data_structure;
} DfsmAstExpressionDataStructure;

DfsmAstExpression *dfsm_ast_expression_data_structure_new (DfsmAstDataStructure *data_structure, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstExpression parent;
	DfsmAstExpression *child_node;
} DfsmAstExpressionUnary;

DfsmAstExpression *dfsm_ast_expression_unary_new (DfsmAstExpressionType expression_type, DfsmAstExpression *child_node, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstExpression parent;
	DfsmAstExpression *left_node;
	DfsmAstExpression *right_node;
} DfsmAstExpressionBinary;

DfsmAstExpression *dfsm_ast_expression_binary_new (DfsmAstExpressionType expression_type, DfsmAstExpression *left_node,
                                                   DfsmAstExpression *right_node, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstExpression *key;
	DfsmAstExpression *value;
} DfsmAstDictionaryEntry;

DfsmAstDictionaryEntry *dfsm_ast_dictionary_entry_new (DfsmAstExpression *key, DfsmAstExpression *value) DFSM_CONSTRUCTOR;
void dfsm_ast_dictionary_entry_free (DfsmAstDictionaryEntry *entry);

typedef enum {
	DFSM_AST_DATA_STRING,
	DFSM_AST_DATA_INTEGER,
	DFSM_AST_DATA_FLOAT,
	DFSM_AST_DATA_BOOLEAN,
	DFSM_AST_DATA_REGEXP,
	DFSM_AST_DATA_ARRAY,
	DFSM_AST_DATA_DICTIONARY,
	DFSM_AST_DATA_VARIABLE,
} DfsmAstDataStructureType;

typedef struct _DfsmAstVariable DfsmAstVariable;

struct _DfsmAstDataStructure {
	DfsmAstNode parent;
	DfsmAstDataStructureType data_structure_type;
	union {
		gchar *str;
		gint64 integer;
		gdouble flt;
		gboolean boolean;
		gchar *regexp;
		GPtrArray *array;
		GPtrArray *dictionary;
		DfsmAstVariable *variable;
	};
};

DfsmAstDataStructure *dfsm_ast_data_structure_new (DfsmAstDataStructureType data_structure_type, gpointer value, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstDataStructure parent;
	gboolean is_fuzzy;
} DfsmAstFuzzyDataStructure;

DfsmAstDataStructure *dfsm_ast_fuzzy_data_structure_new (DfsmAstDataStructure *data_structure, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	gchar *type_string;
	DfsmAstDataStructure *value_expression;
} DfsmAstDataItem;

DfsmAstDataItem *dfsm_ast_data_item_new (const gchar *type_string, DfsmAstDataStructure *value_expression) DFSM_CONSTRUCTOR;
void dfsm_ast_data_item_free (DfsmAstDataItem *data_item);

typedef enum {
	DFSM_AST_TRANSITION_METHOD_CALL,
	DFSM_AST_TRANSITION_ARBITRARY,
} DfsmAstTransitionTrigger;

typedef struct {
	DfsmAstNode parent;
	gchar *from_state_name;
	gchar *to_state_name;
	DfsmAstTransitionTrigger trigger;
	union {
		gchar *method_name; /* for DFSM_AST_TRANSITION_METHOD_CALL, otherwise null */
	} trigger_params;
	GPtrArray *preconditions; /* array of DfsmAstPreconditions */
	GPtrArray *statements; /* array of DfsmAstStatements */
} DfsmAstTransition;

DfsmAstTransition *dfsm_ast_transition_new (const gchar *from_state_name, const gchar *to_state_name, const gchar *transition_type,
                                            GPtrArray/*<DfsmAstPrecondition>*/ *preconditions,
                                            GPtrArray/*<DfsmAstStatement>*/ *statements, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstNode parent;
	gchar *error_name; /* nullable */
	DfsmAstExpression *condition;
} DfsmAstPrecondition;

DfsmAstPrecondition *dfsm_ast_precondition_new (const gchar *error_name /* nullable */, DfsmAstExpression *condition, GError **error) DFSM_CONSTRUCTOR;

typedef enum {
	DFSM_AST_STATEMENT_ASSIGNMENT,
	DFSM_AST_STATEMENT_THROW,
	DFSM_AST_STATEMENT_EMIT,
	DFSM_AST_STATEMENT_REPLY,
} DfsmAstStatementType;

/* abstract */
typedef struct {
	DfsmAstNode parent;
	DfsmAstStatementType statement_type;
} DfsmAstStatement;

typedef struct {
	DfsmAstStatement parent;
	DfsmAstDataStructure *data_structure; /* lvalue */
	DfsmAstExpression *expression; /* rvalue */
} DfsmAstStatementAssignment;

DfsmAstStatement *dfsm_ast_statement_assignment_new (DfsmAstDataStructure *data_structure,
                                                     DfsmAstExpression *expression, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstStatement parent;
	gchar *error_name;
} DfsmAstStatementThrow;

DfsmAstStatement *dfsm_ast_statement_throw_new (const gchar *error_name, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstStatement parent;
	gchar *signal_name;
	DfsmAstExpression *expression;
} DfsmAstStatementEmit;

DfsmAstStatement *dfsm_ast_statement_emit_new (const gchar *signal_name, DfsmAstExpression *expression, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	DfsmAstStatement parent;
	DfsmAstExpression *expression;
} DfsmAstStatementReply;

DfsmAstStatement *dfsm_ast_statement_reply_new (DfsmAstExpression *expression, GError **error) DFSM_CONSTRUCTOR;

typedef enum {
	DFSM_AST_SCOPE_LOCAL,
	DFSM_AST_SCOPE_OBJECT,
} DfsmAstScope;

struct _DfsmAstVariable {
	DfsmAstNode parent;
	DfsmAstScope scope;
	gchar *variable_name;
};

DfsmAstVariable *dfsm_ast_variable_new (DfsmAstScope scope, const gchar *variable_name, GError **error) DFSM_CONSTRUCTOR;

G_END_DECLS

#endif /* !DFSM_AST_H */
