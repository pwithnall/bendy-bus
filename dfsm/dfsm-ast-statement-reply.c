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

#include "dfsm-ast-statement-reply.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_statement_reply_dispose (GObject *object);
static void dfsm_ast_statement_reply_sanity_check (DfsmAstNode *node);
static void dfsm_ast_statement_reply_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_statement_reply_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static GVariant *dfsm_ast_statement_reply_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error);

struct _DfsmAstStatementReplyPrivate {
	DfsmAstExpression *expression;
};

G_DEFINE_TYPE (DfsmAstStatementReply, dfsm_ast_statement_reply, DFSM_TYPE_AST_STATEMENT)

static void
dfsm_ast_statement_reply_class_init (DfsmAstStatementReplyClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);
	DfsmAstStatementClass *statement_class = DFSM_AST_STATEMENT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstStatementReplyPrivate));

	gobject_class->dispose = dfsm_ast_statement_reply_dispose;

	node_class->sanity_check = dfsm_ast_statement_reply_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_statement_reply_pre_check_and_register;
	node_class->check = dfsm_ast_statement_reply_check;

	statement_class->execute = dfsm_ast_statement_reply_execute;
}

static void
dfsm_ast_statement_reply_init (DfsmAstStatementReply *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_STATEMENT_REPLY, DfsmAstStatementReplyPrivate);
}

static void
dfsm_ast_statement_reply_dispose (GObject *object)
{
	DfsmAstStatementReplyPrivate *priv = DFSM_AST_STATEMENT_REPLY (object)->priv;

	g_clear_object (&priv->expression);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_statement_reply_parent_class)->dispose (object);
}

static void
dfsm_ast_statement_reply_sanity_check (DfsmAstNode *node)
{
	DfsmAstStatementReplyPrivate *priv = DFSM_AST_STATEMENT_REPLY (node)->priv;

	g_assert (priv->expression != NULL);
	dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->expression));
}

static void
dfsm_ast_statement_reply_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementReplyPrivate *priv = DFSM_AST_STATEMENT_REPLY (node)->priv;

	dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->expression), environment, error);

	if (*error != NULL) {
		return;
	}

	/* TODO: Check we're actually allowed to reply. */
}

static void
dfsm_ast_statement_reply_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementReplyPrivate *priv = DFSM_AST_STATEMENT_REPLY (node)->priv;

	dfsm_ast_node_check (DFSM_AST_NODE (priv->expression), environment, error);

	if (*error != NULL) {
		return;
	}

	/* TODO: Check expression's type matches the method. */
}

static GVariant *
dfsm_ast_statement_reply_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementReplyPrivate *priv = DFSM_AST_STATEMENT_REPLY (statement)->priv;
	GVariant *value;
	GError *child_error = NULL;

	/* Evaluate the expression */
	value = dfsm_ast_expression_evaluate (priv->expression, environment, &child_error);
	g_assert ((value == NULL) != (child_error == NULL));

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	return value;
}

/**
 * dfsm_ast_statement_reply_new:
 * @expression: expression to evaluate as the reply value
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstStatement for replying to the triggering D-Bus method call with value given by @expression.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_reply_new (DfsmAstExpression *expression, GError **error)
{
	DfsmAstStatementReply *statement;
	DfsmAstStatementReplyPrivate *priv;

	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (expression), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	statement = g_object_new (DFSM_TYPE_AST_STATEMENT_REPLY, NULL);
	priv = statement->priv;

	priv->expression = g_object_ref (expression);

	return DFSM_AST_STATEMENT (statement);
}
