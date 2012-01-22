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
 *
 * Execute a given state machine statement. This may modify the @environment.
 *
 * If the statement is successful (i.e. a D-Bus reply is the result), the parameters of the reply will be returned. If the statement is unsuccessful
 * (i.e. a D-Bus error is thrown) the error will be returned in @error.
 *
 * Return value: (transfer full): reply parameters from the statement, or %NULL
 */
GVariant *
dfsm_ast_statement_execute (DfsmAstStatement *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementClass *klass;
	GVariant *return_value;
	GError *child_error = NULL;

	g_return_val_if_fail (DFSM_IS_AST_STATEMENT (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);

	klass = DFSM_AST_STATEMENT_GET_CLASS (self);

	g_assert (klass->execute != NULL);
	return_value = klass->execute (self, environment, &child_error);

	g_assert (return_value == NULL || child_error == NULL);
	g_assert (return_value == NULL || g_variant_is_floating (return_value) == FALSE);

	/* Use our own child_error so that we can guarantee the error is non-NULL in implementations of the virtual method. */
	if (child_error != NULL) {
		g_propagate_error (error, child_error);
	}

	return return_value;
}
