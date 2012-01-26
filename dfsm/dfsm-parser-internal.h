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

#ifndef DFSM_PARSER_INTERNAL_H
#define DFSM_PARSER_INTERNAL_H

#include <glib.h>
#include <gio/gio.h>

#include "dfsm-machine.h"
#include "dfsm-utils.h"

G_BEGIN_DECLS

typedef struct {
	/* Gubbins */
	void *yyscanner;
	GDBusNodeInfo *dbus_node_info;

	/* Parser output */
	GPtrArray *object_array;

	/* Source code to parse. */
	const gchar *source_buf; /* UTF-8 */
	glong source_len; /* in characters, not bytes */
	glong source_pos; /* in characters, not bytes */
} DfsmParserData;

G_GNUC_INTERNAL GPtrArray *dfsm_bison_parse (GDBusNodeInfo *dbus_node_info, const gchar *source_buf, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	GPtrArray *data_blocks; /* array of GHashTables */
	GPtrArray *state_blocks; /* array of GPtrArrays of states */
	GPtrArray *transitions; /* array of DfsmParserTransitionBlocks */
} DfsmParserBlockList;

G_GNUC_INTERNAL DfsmParserBlockList *dfsm_parser_block_list_new (void) DFSM_CONSTRUCTOR;
G_GNUC_INTERNAL void dfsm_parser_block_list_free (DfsmParserBlockList *block_list);

typedef enum {
	DFSM_PARSER_TRANSITION_METHOD_CALL,
	DFSM_PARSER_TRANSITION_PROPERTY_SET,
	DFSM_PARSER_TRANSITION_ARBITRARY,
} DfsmParserTransitionType;

typedef struct {
	DfsmParserTransitionType transition_type;
	gchar *str;
} DfsmParserTransitionDetails;

G_GNUC_INTERNAL DfsmParserTransitionDetails *dfsm_parser_transition_details_new (DfsmParserTransitionType transition_type,
                                                                                 const gchar *str) DFSM_CONSTRUCTOR;
G_GNUC_INTERNAL void dfsm_parser_transition_details_free (DfsmParserTransitionDetails *details);

#include "dfsm-ast-transition.h"

typedef struct {
	DfsmAstTransition *transition;
	GPtrArray/*<DfsmParserStatePair>*/ *state_pairs;
} DfsmParserTransitionBlock;

G_GNUC_INTERNAL DfsmParserTransitionBlock *dfsm_parser_transition_block_new (DfsmAstTransition *transition,
                                                                             GPtrArray/*<DfsmParserStatePair>*/ *state_pairs) DFSM_CONSTRUCTOR;
G_GNUC_INTERNAL void dfsm_parser_transition_block_free (DfsmParserTransitionBlock *block);

typedef struct {
	gchar *from_state_name;
	gchar *to_state_name;
	gchar *nickname; /* may be NULL */
} DfsmParserStatePair;

G_GNUC_INTERNAL DfsmParserStatePair *dfsm_parser_state_pair_new (const gchar *from_state_name, const gchar *to_state_name,
                                                                 const gchar *nickname) DFSM_CONSTRUCTOR;
G_GNUC_INTERNAL void dfsm_parser_state_pair_free (DfsmParserStatePair *state_pair);

/* AST node constructors */
G_GNUC_INTERNAL DfsmEnvironment *_dfsm_environment_new (GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces) DFSM_CONSTRUCTOR;
G_GNUC_INTERNAL DfsmMachine *_dfsm_machine_new (DfsmEnvironment *environment, GPtrArray/*<string>*/ *state_names,
                                                GPtrArray/*<DfsmAstTransition>*/ *transitions) DFSM_CONSTRUCTOR;

#include "dfsm-ast-data-structure.h"

G_GNUC_INTERNAL DfsmAstDataStructure *dfsm_ast_data_structure_new (DfsmAstDataStructureType data_structure_type, gpointer value) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL void dfsm_ast_data_structure_set_weight (DfsmAstDataStructure *self, gdouble weight);
G_GNUC_INTERNAL void dfsm_ast_data_structure_set_type_annotation (DfsmAstDataStructure *self, const gchar *type_annotation);

#include "dfsm-ast-expression-binary.h"

G_GNUC_INTERNAL DfsmAstExpression *dfsm_ast_expression_binary_new (DfsmAstExpressionBinaryType expression_type, DfsmAstExpression *left_node,
                                                                   DfsmAstExpression *right_node) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmAstExpression *dfsm_ast_expression_data_structure_new (DfsmAstDataStructure *data_structure) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmAstExpression *dfsm_ast_expression_function_call_new (const gchar *function_name, DfsmAstExpression *parameters) DFSM_CONSTRUCTOR;

#include "dfsm-ast-expression-unary.h"

G_GNUC_INTERNAL DfsmAstExpression *dfsm_ast_expression_unary_new (DfsmAstExpressionUnaryType expression_type,
                                                                  DfsmAstExpression *child_node) DFSM_CONSTRUCTOR;

#include "dfsm-ast-object.h"

G_GNUC_INTERNAL DfsmAstObject *dfsm_ast_object_new (GDBusNodeInfo *dbus_node_info, const gchar *object_path, GPtrArray/*<string>*/ *bus_names,
                                                    GPtrArray/*<string>*/ *interface_names,
                                                    GPtrArray/*<GHashTable>*/ *data_blocks, GPtrArray/*<GPtrArray>*/ *state_blocks,
                                                    GPtrArray/*<DfsmParserTransitionBlock>*/ *transition_blocks) DFSM_CONSTRUCTOR;

#include "dfsm-ast-precondition.h"

G_GNUC_INTERNAL DfsmAstPrecondition *dfsm_ast_precondition_new (const gchar *error_name /* nullable */, DfsmAstExpression *condition) DFSM_CONSTRUCTOR;

#include "dfsm-ast-statement.h"

G_GNUC_INTERNAL DfsmAstStatement *dfsm_ast_statement_assignment_new (DfsmAstDataStructure *data_structure,
                                                                     DfsmAstExpression *expression) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmAstStatement *dfsm_ast_statement_emit_new (const gchar *signal_name, DfsmAstExpression *expression) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmAstStatement *dfsm_ast_statement_reply_new (DfsmAstExpression *expression) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmAstStatement *dfsm_ast_statement_throw_new (const gchar *error_name) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmAstTransition *dfsm_ast_transition_new (const DfsmParserTransitionDetails *details,
                                                            GPtrArray/*<DfsmAstPrecondition>*/ *preconditions,
                                                            GPtrArray/*<DfsmAstStatement>*/ *statements) DFSM_CONSTRUCTOR;

#include "dfsm-ast-variable.h"

G_GNUC_INTERNAL DfsmAstVariable *dfsm_ast_variable_new (DfsmVariableScope scope, const gchar *variable_name) DFSM_CONSTRUCTOR;

G_END_DECLS

#endif /* !DFSM_PARSER_INTERNAL_H */
