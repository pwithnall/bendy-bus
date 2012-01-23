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

#include "dfsm-ast-statement.h"

G_DEFINE_ABSTRACT_TYPE (DfsmAstStatement, dfsm_ast_statement, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_statement_class_init (DfsmAstStatementClass *klass)
{
	/* Nothing to see here. */
}

static void
dfsm_ast_statement_init (DfsmAstStatement *self)
{
	/* Nothing to see here. */
}

/**
 * dfsm_ast_statement_execute:
 * @self: a #DfsmAstStatement
 * @environment: the environment to execute the statement in
 * @output_sequence: an output sequence to append the effects of the statement to
 *
 * Execute a given state machine statement. This may modify the @environment.
 *
 * Any effects caused by execution of the statement will be appended (in execution order) to @output_sequence, which can later be evaluated by the
 * caller. For example, all D-Bus signal emissions, method replies and error replies to D-Bus method calls are appended to @output_sequence in this
 * manner.
 *
 * Return value: (transfer full): reply parameters from the statement, or %NULL
 */
void
dfsm_ast_statement_execute (DfsmAstStatement *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence)
{
	DfsmAstStatementClass *klass;

	g_return_if_fail (DFSM_IS_AST_STATEMENT (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));

	klass = DFSM_AST_STATEMENT_GET_CLASS (self);

	g_assert (klass->execute != NULL);
	klass->execute (self, environment, output_sequence);
}
