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

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-ast-statement-throw.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_statement_throw_finalize (GObject *object);
static void dfsm_ast_statement_throw_sanity_check (DfsmAstNode *node);
static void dfsm_ast_statement_throw_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_statement_throw_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence);

struct _DfsmAstStatementThrowPrivate {
	gchar *error_name;
};

G_DEFINE_TYPE (DfsmAstStatementThrow, dfsm_ast_statement_throw, DFSM_TYPE_AST_STATEMENT)

static void
dfsm_ast_statement_throw_class_init (DfsmAstStatementThrowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);
	DfsmAstStatementClass *statement_class = DFSM_AST_STATEMENT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstStatementThrowPrivate));

	gobject_class->finalize = dfsm_ast_statement_throw_finalize;

	node_class->sanity_check = dfsm_ast_statement_throw_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_statement_throw_pre_check_and_register;

	statement_class->execute = dfsm_ast_statement_throw_execute;
}

static void
dfsm_ast_statement_throw_init (DfsmAstStatementThrow *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_STATEMENT_THROW, DfsmAstStatementThrowPrivate);
}

static void
dfsm_ast_statement_throw_finalize (GObject *object)
{
	DfsmAstStatementThrowPrivate *priv = DFSM_AST_STATEMENT_THROW (object)->priv;

	g_free (priv->error_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_statement_throw_parent_class)->finalize (object);
}

static void
dfsm_ast_statement_throw_sanity_check (DfsmAstNode *node)
{
	DfsmAstStatementThrowPrivate *priv = DFSM_AST_STATEMENT_THROW (node)->priv;

	g_assert (priv->error_name != NULL);
}

static void
dfsm_ast_statement_throw_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementThrowPrivate *priv = DFSM_AST_STATEMENT_THROW (node)->priv;

	if (g_dbus_is_member_name (priv->error_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus error name: %s"), priv->error_name);
		return;
	}

	/* Whether the transition we're in is allowed to throw errors is checked by the transition itself, not us. */
}

static void
dfsm_ast_statement_throw_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence)
{
	DfsmAstStatementThrowPrivate *priv = DFSM_AST_STATEMENT_THROW (statement)->priv;
	gchar *message;
	GError *child_error = NULL;

	message = g_strdup_printf (_("Error message generated by %s()."), G_STRFUNC);
	g_dbus_error_set_dbus_error (&child_error, priv->error_name, message, NULL);
	g_free (message);

	dfsm_output_sequence_add_throw (output_sequence, child_error);

	g_error_free (child_error);
}

/**
 * dfsm_ast_statement_throw_new:
 * @error_name: name of the D-Bus error to throw
 *
 * Create a new #DfsmAstStatement for throwing a D-Bus error of type @error_name.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_throw_new (const gchar *error_name)
{
	DfsmAstStatementThrow *statement;
	DfsmAstStatementThrowPrivate *priv;

	g_return_val_if_fail (error_name != NULL && *error_name != '\0', NULL);

	statement = g_object_new (DFSM_TYPE_AST_STATEMENT_THROW, NULL);
	priv = statement->priv;

	priv->error_name = g_strdup (error_name);

	return DFSM_AST_STATEMENT (statement);
}
