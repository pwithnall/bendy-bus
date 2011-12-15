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

#include "dfsm-ast.h"
#include "dfsm-parser.h"

GQuark
dfsm_parse_error_quark (void)
{
	return g_quark_from_static_string ("dfsm-parse-error-quark");
}

DfsmParserBlockList *
dfsm_parser_block_list_new (void)
{
	DfsmParserBlockList *block_list = g_slice_new (DfsmParserBlockList);

	block_list->data_blocks = g_ptr_array_new_with_free_func ((GDestroyNotify) g_hash_table_unref);
	block_list->state_blocks = g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);
	block_list->transitions = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_parser_transition_block_free);

	return block_list;
}

void
dfsm_parser_block_list_free (DfsmParserBlockList *block_list)
{
	if (block_list != NULL) {
		g_ptr_array_unref (block_list->data_blocks);
		g_ptr_array_unref (block_list->state_blocks);
		g_ptr_array_unref (block_list->transitions);

		g_slice_free (DfsmParserBlockList, block_list);
	}
}

DfsmParserTransitionDetails *
dfsm_parser_transition_details_new (DfsmParserTransitionType transition_type, const gchar *str)
{
	DfsmParserTransitionDetails *details = g_slice_new (DfsmParserTransitionDetails);

	details->transition_type = transition_type;
	details->str = g_strdup (str);

	return details;
}

void
dfsm_parser_transition_details_free (DfsmParserTransitionDetails *details)
{
	if (details != NULL) {
		g_free (details->str);

		g_slice_free (DfsmParserTransitionDetails, details);
	}
}

DfsmParserTransitionBlock *
dfsm_parser_transition_block_new (DfsmAstTransition *transition, GPtrArray/*<DfsmParserStatePair>*/ *state_pairs)
{
	DfsmParserTransitionBlock *block = g_slice_new (DfsmParserTransitionBlock);

	block->transition = g_object_ref (transition);
	block->state_pairs = g_ptr_array_ref (state_pairs);

	return block;
}

void
dfsm_parser_transition_block_free (DfsmParserTransitionBlock *block)
{
	if (block != NULL) {
		g_ptr_array_unref (block->state_pairs);
		g_object_unref (block->transition);

		g_slice_free (DfsmParserTransitionBlock, block);
	}
}

DfsmParserStatePair *
dfsm_parser_state_pair_new (const gchar *from_state_name, const gchar *to_state_name)
{
	DfsmParserStatePair *state_pair = g_slice_new (DfsmParserStatePair);

	/* Allow null transitions. */
	if (from_state_name == NULL) {
		from_state_name = to_state_name;
	}

	state_pair->from_state_name = g_strdup (from_state_name);
	state_pair->to_state_name = g_strdup (to_state_name);

	return state_pair;
}

void
dfsm_parser_state_pair_free (DfsmParserStatePair *state_pair)
{
	if (state_pair != NULL) {
		g_free (state_pair->from_state_name);
		g_free (state_pair->to_state_name);

		g_slice_free (DfsmParserStatePair, state_pair);
	}
}

static gboolean
_is_member_name (const gchar *member_name)
{
	const gchar *i;

	g_return_val_if_fail (member_name != NULL, FALSE);

	/* Characters outside [A-Za-z0-9_]? */
	for (i = member_name; *i != '\0'; i++) {
		if (*i != '_' && g_ascii_isalnum (*i) == FALSE) {
			return FALSE;
		}
	}

	/* Zero-length string? */
	if (i == member_name) {
		return FALSE;
	}

	return TRUE;
}

/**
 * dfsm_is_variable_name:
 * @variable_name: variable name to check
 *
 * Checks whether @variable_name is a valid variable name in the FSM language. Valid variable names conform to the restrictions on D-Bus member names,
 * as specified in the <ulink url="http://dbus.freedesktop.org/doc/dbus-specification.html#sect3" type="http">D-Bus Specification</ulink>,
 * § Member names.
 *
 * Return value: %TRUE if @variable_name is a valid variable name; %FALSE otherwise
 */
gboolean
dfsm_is_variable_name (const gchar *variable_name)
{
	return _is_member_name (variable_name);
}

/**
 * dfsm_is_state_name:
 * @state_name: state name to check
 *
 * Checks whether @state_name is a valid state name in the FSM language. Valid state names conform to the restrictions on D-Bus member names,
 * as specified in the <ulink url="http://dbus.freedesktop.org/doc/dbus-specification.html#sect3" type="http">D-Bus Specification</ulink>,
 * § Member names.
 *
 * Return value: %TRUE if @state_name is a valid state name; %FALSE otherwise
 */
gboolean
dfsm_is_state_name (const gchar *state_name)
{
	return _is_member_name (state_name);
}

/**
 * dfsm_is_function_name:
 * @function_name: function name to check
 *
 * Checks whether @function_name is a valid function name in the FSM language. Valid function names conform to the restrictions on D-Bus member names,
 * as specified in the <ulink url="http://dbus.freedesktop.org/doc/dbus-specification.html#sect3" type="http">D-Bus Specification</ulink>,
 * § Member names.
 *
 * Return value: %TRUE if @function_name is a valid function name; %FALSE otherwise
 */
gboolean
dfsm_is_function_name (const gchar *function_name)
{
	return _is_member_name (function_name);
}
