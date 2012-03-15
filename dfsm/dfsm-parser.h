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
 * SECTION:dfsm-parser
 * @short_description: parser utilities
 * @stability: Unstable
 * @include: dfsm/dfsm-parser.h
 *
 * Parser utilities and error handling. The actual parser is implemented as dfsm_object_factory_from_files() and friends.
 */

#ifndef DFSM_PARSER_H
#define DFSM_PARSER_H

#include <glib.h>
#include <gio/gio.h>

#include "dfsm-machine.h"
#include "dfsm-utils.h"

G_BEGIN_DECLS

/**
 * DfsmParseError:
 * @DFSM_PARSE_ERROR_SYNTAX: syntax error in the input file
 * @DFSM_PARSE_ERROR_OOM: out of memory while parsing
 * @DFSM_PARSE_ERROR_AST_INVALID: post-parsing checks on the AST failed
 *
 * Error codes for the #DFSM_PARSE_ERROR domain returned by the parser.
 */
typedef enum {
	DFSM_PARSE_ERROR_SYNTAX,
	DFSM_PARSE_ERROR_OOM,
	DFSM_PARSE_ERROR_AST_INVALID,
} DfsmParseError;

/**
 * DFSM_PARSE_ERROR:
 *
 * Error domain returned by the parser.
 */
#define DFSM_PARSE_ERROR dfsm_parse_error_quark ()

GQuark dfsm_parse_error_quark (void) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_PARSER_H */
