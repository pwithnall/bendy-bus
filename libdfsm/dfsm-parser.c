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

DfsmParserBlockList *
dfsm_parser_block_list_new (void)
{
	DfsmParserBlockList *block_list = g_slice_new (DfsmParserBlockList);

	block_list->data_blocks = g_ptr_array_new_with_free_func ((GDestroyNotify) g_hash_table_unref);
	block_list->state_blocks = g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);
	block_list->transitions = g_ptr_array_new_with_free_func (dfsm_ast_node_unref);

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
