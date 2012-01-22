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

#include "dfsm-ast-statement-emit.h"
#include "dfsm-internal.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

static void dfsm_ast_statement_emit_dispose (GObject *object);
static void dfsm_ast_statement_emit_finalize (GObject *object);
static void dfsm_ast_statement_emit_sanity_check (DfsmAstNode *node);
static void dfsm_ast_statement_emit_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_statement_emit_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static GVariant *dfsm_ast_statement_emit_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error);

struct _DfsmAstStatementEmitPrivate {
	gchar *signal_name;
	DfsmAstExpression *expression;
};

G_DEFINE_TYPE (DfsmAstStatementEmit, dfsm_ast_statement_emit, DFSM_TYPE_AST_STATEMENT)

static void
dfsm_ast_statement_emit_class_init (DfsmAstStatementEmitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);
	DfsmAstStatementClass *statement_class = DFSM_AST_STATEMENT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstStatementEmitPrivate));

	gobject_class->dispose = dfsm_ast_statement_emit_dispose;
	gobject_class->finalize = dfsm_ast_statement_emit_finalize;

	node_class->sanity_check = dfsm_ast_statement_emit_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_statement_emit_pre_check_and_register;
	node_class->check = dfsm_ast_statement_emit_check;

	statement_class->execute = dfsm_ast_statement_emit_execute;
}

static void
dfsm_ast_statement_emit_init (DfsmAstStatementEmit *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_STATEMENT_EMIT, DfsmAstStatementEmitPrivate);
}

static void
dfsm_ast_statement_emit_dispose (GObject *object)
{
	DfsmAstStatementEmitPrivate *priv = DFSM_AST_STATEMENT_EMIT (object)->priv;

	g_clear_object (&priv->expression);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_statement_emit_parent_class)->dispose (object);
}

static void
dfsm_ast_statement_emit_finalize (GObject *object)
{
	DfsmAstStatementEmitPrivate *priv = DFSM_AST_STATEMENT_EMIT (object)->priv;

	g_free (priv->signal_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_statement_emit_parent_class)->finalize (object);
}

static void
dfsm_ast_statement_emit_sanity_check (DfsmAstNode *node)
{
	DfsmAstStatementEmitPrivate *priv = DFSM_AST_STATEMENT_EMIT (node)->priv;

	g_assert (priv->signal_name != NULL);

	g_assert (priv->expression != NULL);
	dfsm_ast_node_sanity_check (DFSM_AST_NODE (priv->expression));
}

static void
dfsm_ast_statement_emit_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementEmitPrivate *priv = DFSM_AST_STATEMENT_EMIT (node)->priv;

	if (g_dbus_is_member_name (priv->signal_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus signal name: %s"), priv->signal_name);
		return;
	}

	dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (priv->expression), environment, error);

	if (*error != NULL) {
		return;
	}
}

static void
dfsm_ast_statement_emit_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementEmitPrivate *priv = DFSM_AST_STATEMENT_EMIT (node)->priv;
	GDBusSignalInfo *signal_info = NULL;
	GVariantType *expr_parameters_type, *signal_parameters_type;
	GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;
	guint i;

	dfsm_ast_node_check (DFSM_AST_NODE (priv->expression), environment, error);

	if (*error != NULL) {
		return;
	}

	/* Find the interface declaring the signal out of the interfaces declared as implemented by the object */
	interfaces = dfsm_environment_get_interfaces (environment);

	for (i = 0; i < interfaces->len; i++) {
		GDBusInterfaceInfo *interface_info = (GDBusInterfaceInfo*) g_ptr_array_index (interfaces, i);

		signal_info = g_dbus_interface_info_lookup_signal (interface_info, priv->signal_name);

		if (signal_info != NULL) {
			/* Found the interface defining signal_name. */
			break;
		}
	}

	/* Failed to find a suitable interface? */
	if (signal_info == NULL) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Undeclared D-Bus signal referenced by an ‘emit’ statement: %s"), priv->signal_name);
		return;
	}

	/* Check the expression's type is a tuple. */
	expr_parameters_type = dfsm_ast_expression_calculate_type (priv->expression, environment);
	signal_parameters_type = dfsm_internal_dbus_arg_info_array_to_variant_type ((const GDBusArgInfo**) signal_info->args);

	if (g_variant_type_is_subtype_of (expr_parameters_type, signal_parameters_type) == FALSE) {
		gchar *expr_parameters_type_string, *signal_parameters_type_string;

		expr_parameters_type_string = g_variant_type_dup_string (expr_parameters_type);
		signal_parameters_type_string = g_variant_type_dup_string (signal_parameters_type);

		g_variant_type_free (signal_parameters_type);
		g_variant_type_free (expr_parameters_type);

		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("Type mismatch between formal and actual parameters to D-Bus signal ‘%s’: expects type ‘%s’ but received type ‘%s’."),
		             priv->signal_name, signal_parameters_type_string, expr_parameters_type_string);

		g_free (signal_parameters_type_string);
		g_free (expr_parameters_type_string);

		return;
	}

	g_variant_type_free (signal_parameters_type);
	g_variant_type_free (expr_parameters_type);
}

static GVariant *
dfsm_ast_statement_emit_execute (DfsmAstStatement *statement, DfsmEnvironment *environment, GError **error)
{
	DfsmAstStatementEmitPrivate *priv = DFSM_AST_STATEMENT_EMIT (statement)->priv;
	GVariant *expression_value;

	/* Evaluate the child expression first. */
	expression_value = dfsm_ast_expression_evaluate (priv->expression, environment);

	/* Emit the signal. */
	dfsm_environment_emit_signal (environment, priv->signal_name, expression_value);

	g_variant_unref (expression_value);

	return NULL;
}

/**
 * dfsm_ast_statement_emit_new:
 * @signal_name: name of the D-Bus signal to emit
 * @expression: expression to evaluate as the signal parameters
 *
 * Create a new #DfsmAstStatement for emitting a D-Bus signal of name @signal_name with value given by @expression.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstStatement *
dfsm_ast_statement_emit_new (const gchar *signal_name, DfsmAstExpression *expression)
{
	DfsmAstStatementEmit *statement;
	DfsmAstStatementEmitPrivate *priv;

	g_return_val_if_fail (signal_name != NULL && *signal_name != '\0', NULL);
	g_return_val_if_fail (DFSM_IS_AST_EXPRESSION (expression), NULL);

	statement = g_object_new (DFSM_TYPE_AST_STATEMENT_EMIT, NULL);
	priv = statement->priv;

	priv->signal_name = g_strdup (signal_name);
	priv->expression = g_object_ref (expression);

	return DFSM_AST_STATEMENT (statement);
}
