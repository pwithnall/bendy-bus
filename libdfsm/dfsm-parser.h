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

#include "dfsm-utils.h"

G_BEGIN_DECLS

typedef struct {
	/* Gubbins */
	void *yyscanner;

	/* Parser output */
	GPtrArray *object_array;
	GError *error;

	/* Source code to parse. */
	const gchar *source_buf; /* UTF-8 */
	glong source_len; /* in characters, not bytes */
	glong source_pos; /* in characters, not bytes */
} DfsmParserData;

/* TODO: This should not be exported */
GPtrArray *dfsm_bison_parse (const gchar *source_buf, GError **error) DFSM_CONSTRUCTOR;

typedef struct {
	GPtrArray *data_blocks; /* array of GHashTables */
	GPtrArray *state_blocks; /* array of GPtrArrays of states */
	GPtrArray *transitions; /* array of DfsmAstTransitions */
} DfsmParserBlockList;

DfsmParserBlockList *dfsm_parser_block_list_new (void) DFSM_CONSTRUCTOR;
void dfsm_parser_block_list_free (DfsmParserBlockList *block_list);

G_END_DECLS

#endif /* !DFSM_PARSER_H */
