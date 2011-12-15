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

#ifndef DFSM_PARSER_H
#define DFSM_PARSER_H

#include <glib.h>
#include <gio/gio.h>

#include "dfsm-machine.h"
#include "dfsm-utils.h"

G_BEGIN_DECLS

typedef enum {
	DFSM_PARSE_ERROR_SYNTAX,
	DFSM_PARSE_ERROR_OOM,
	DFSM_PARSE_ERROR_AST_INVALID,
} DfsmParseError;

#define DFSM_PARSE_ERROR dfsm_parse_error_quark ()
GQuark dfsm_parse_error_quark (void) G_GNUC_PURE;

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

/* TODO: This should not be exported */
GPtrArray *dfsm_bison_parse (GDBusNodeInfo *dbus_node_info, const gchar *source_buf, GError **error) DFSM_CONSTRUCTOR;

G_GNUC_INTERNAL DfsmEnvironment *_dfsm_environment_new (GDBusNodeInfo *dbus_node_info) DFSM_CONSTRUCTOR;
G_GNUC_INTERNAL DfsmMachine *_dfsm_machine_new (DfsmEnvironment *environment, GPtrArray/*<string>*/ *state_names,
                                                GPtrArray/*<DfsmAstTransition>*/ *transitions) DFSM_CONSTRUCTOR;

typedef struct {
	GPtrArray *data_blocks; /* array of GHashTables */
	GPtrArray *state_blocks; /* array of GPtrArrays of states */
	GPtrArray *transitions; /* array of DfsmParserTransitionBlocks */
} DfsmParserBlockList;

DfsmParserBlockList *dfsm_parser_block_list_new (void) DFSM_CONSTRUCTOR;
void dfsm_parser_block_list_free (DfsmParserBlockList *block_list);

typedef enum {
	DFSM_PARSER_TRANSITION_METHOD_CALL,
	DFSM_PARSER_TRANSITION_PROPERTY_SET,
	DFSM_PARSER_TRANSITION_ARBITRARY,
} DfsmParserTransitionType;

typedef struct {
	DfsmParserTransitionType transition_type;
	gchar *str;
} DfsmParserTransitionDetails;

DfsmParserTransitionDetails *dfsm_parser_transition_details_new (DfsmParserTransitionType transition_type, const gchar *str) DFSM_CONSTRUCTOR;
void dfsm_parser_transition_details_free (DfsmParserTransitionDetails *details);

#include "dfsm-ast-transition.h"

typedef struct {
	DfsmAstTransition *transition;
	GPtrArray/*<DfsmParserStatePair>*/ *state_pairs;
} DfsmParserTransitionBlock;

DfsmParserTransitionBlock *dfsm_parser_transition_block_new (DfsmAstTransition *transition,
                                                             GPtrArray/*<DfsmParserStatePair>*/ *state_pairs) DFSM_CONSTRUCTOR;
void dfsm_parser_transition_block_free (DfsmParserTransitionBlock *block);

typedef struct {
	gchar *from_state_name;
	gchar *to_state_name;
} DfsmParserStatePair;

DfsmParserStatePair *dfsm_parser_state_pair_new (const gchar *from_state_name, const gchar *to_state_name) DFSM_CONSTRUCTOR;
void dfsm_parser_state_pair_free (DfsmParserStatePair *state_pair);

gboolean dfsm_is_variable_name (const gchar *variable_name) G_GNUC_PURE;
gboolean dfsm_is_state_name (const gchar *state_name) G_GNUC_PURE;
gboolean dfsm_is_function_name (const gchar *function_name) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_PARSER_H */
