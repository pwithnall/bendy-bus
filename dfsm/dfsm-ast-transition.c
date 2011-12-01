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

#include <string.h>
#include <glib.h>

#include "dfsm-ast-precondition.h"
#include "dfsm-ast-statement.h"
#include "dfsm-ast-transition.h"
#include "dfsm-parser.h"
#include "dfsm-utils.h"

static void dfsm_ast_transition_dispose (GObject *object);
static void dfsm_ast_transition_finalize (GObject *object);
static void dfsm_ast_transition_sanity_check (DfsmAstNode *node);
static void dfsm_ast_transition_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_transition_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstTransitionPrivate {
	gchar *from_state_name;
	gchar *to_state_name;
	DfsmAstTransitionTrigger trigger;
	union {
		gchar *method_name; /* for DFSM_AST_TRANSITION_METHOD_CALL, otherwise NULL */
	} trigger_params;
	GPtrArray *preconditions; /* array of DfsmAstPreconditions */
	GPtrArray *statements; /* array of DfsmAstStatements */
};

G_DEFINE_TYPE (DfsmAstTransition, dfsm_ast_transition, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_transition_class_init (DfsmAstTransitionClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstTransitionPrivate));

	gobject_class->dispose = dfsm_ast_transition_dispose;
	gobject_class->finalize = dfsm_ast_transition_finalize;

	node_class->sanity_check = dfsm_ast_transition_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_transition_pre_check_and_register;
	node_class->check = dfsm_ast_transition_check;
}

static void
dfsm_ast_transition_init (DfsmAstTransition *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_TRANSITION, DfsmAstTransitionPrivate);
}

static void
dfsm_ast_transition_dispose (GObject *object)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (object)->priv;

	if (priv->statements != NULL) {
		g_ptr_array_unref (priv->statements);
		priv->statements = NULL;
	}

	if (priv->preconditions != NULL) {
		g_ptr_array_unref (priv->preconditions);
		priv->preconditions = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_transition_parent_class)->dispose (object);
}

static void
dfsm_ast_transition_finalize (GObject *object)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (object)->priv;

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			g_free (priv->trigger_params.method_name);
			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to free here */
			break;
		default:
			g_assert_not_reached ();
	}

	g_free (priv->to_state_name);
	g_free (priv->from_state_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_transition_parent_class)->finalize (object);
}

static void
dfsm_ast_transition_sanity_check (DfsmAstNode *node)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (node)->priv;
	guint i;

	g_assert (priv->from_state_name != NULL);
	g_assert (priv->to_state_name != NULL);
	g_assert (priv->preconditions != NULL);
	g_assert (priv->statements != NULL);

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			g_assert (priv->trigger_params.method_name != NULL);
			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	for (i = 0; i < priv->preconditions->len; i++) {
		g_assert (g_ptr_array_index (priv->preconditions, i) != NULL);
	}

	for (i = 0; i < priv->statements->len; i++) {
		g_assert (g_ptr_array_index (priv->statements, i) != NULL);
	}
}

static void
dfsm_ast_transition_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (node)->priv;
	guint i;

	if (dfsm_is_state_name (priv->from_state_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid state name: %s", priv->from_state_name);
		return;
	} else if (dfsm_is_state_name (priv->to_state_name) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid state name: %s", priv->to_state_name);
		return;
	}

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			if (g_dbus_is_member_name (priv->trigger_params.method_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus method name: %s",
				             priv->trigger_params.method_name);
				return;
			}

			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	for (i = 0; i < priv->preconditions->len; i++) {
		DfsmAstPrecondition *precondition;

		precondition = DFSM_AST_PRECONDITION (g_ptr_array_index (priv->preconditions, i));

		dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (precondition), environment, error);

		if (*error != NULL) {
			return;
		}
	}

	for (i = 0; i < priv->statements->len; i++) {
		DfsmAstStatement *statement;

		statement = DFSM_AST_STATEMENT (g_ptr_array_index (priv->statements, i));

		dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (statement), environment, error);

		if (*error != NULL) {
			return;
		}
	}
}

static void
dfsm_ast_transition_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (node)->priv;
	guint i;

	/* TODO: Check two state names exist */

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			/* TODO: Check method_name exists. */

			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	for (i = 0; i < priv->preconditions->len; i++) {
		DfsmAstPrecondition *precondition;

		precondition = DFSM_AST_PRECONDITION (g_ptr_array_index (priv->preconditions, i));

		dfsm_ast_node_check (DFSM_AST_NODE (precondition), environment, error);

		if (*error != NULL) {
			return;
		}
	}

	for (i = 0; i < priv->statements->len; i++) {
		DfsmAstStatement *statement;

		statement = DFSM_AST_STATEMENT (g_ptr_array_index (priv->statements, i));

		dfsm_ast_node_check (DFSM_AST_NODE (statement), environment, error);

		if (*error != NULL) {
			return;
		}
	}
}

/**
 * dfsm_ast_transition_new:
 * @from_state_name: name of the FSM state being transitioned out of
 * @to_state_name: name of the FSM state being transitioned into
 * @transition_type: method name of the transition trigger, or ‘*’ for an arbitrary transition
 * @precondition: array of #DfsmAstPrecondition<!-- -->s for the transition
 * @statements: array of #DfsmAstStatement<!-- -->s to execute with the transition
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstTransition representing a single transition from @from_state_name to @to_state_name.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstTransition *
dfsm_ast_transition_new (const gchar *from_state_name, const gchar *to_state_name, const gchar *transition_type,
                         GPtrArray/*<DfsmAstPrecondition>*/ *preconditions, GPtrArray/*<DfsmAstStatement>*/ *statements, GError **error)
{
	DfsmAstTransition *transition;
	DfsmAstTransitionPrivate *priv;

	g_return_val_if_fail (from_state_name != NULL && *from_state_name != '\0', NULL);
	g_return_val_if_fail (to_state_name != NULL && *to_state_name != '\0', NULL);
	g_return_val_if_fail (transition_type != NULL && *transition_type != '\0', NULL);
	g_return_val_if_fail (preconditions != NULL, NULL);
	g_return_val_if_fail (statements != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	transition = g_object_new (DFSM_TYPE_AST_TRANSITION, NULL);
	priv = transition->priv;

	priv->from_state_name = g_strdup (from_state_name);
	priv->to_state_name = g_strdup (to_state_name);

	if (strcmp (transition_type, "*") == 0) {
		priv->trigger = DFSM_AST_TRANSITION_ARBITRARY;
	} else {
		priv->trigger = DFSM_AST_TRANSITION_METHOD_CALL;
		priv->trigger_params.method_name = g_strdup (transition_type);
	}

	priv->preconditions = g_ptr_array_ref (preconditions);
	priv->statements = g_ptr_array_ref (statements);

	return transition;
}

/**
 * dfsm_ast_transition_check_preconditions:
 * @self: a #DfsmAstTransition
 * @environment: the environment to execute the transition in
 * @error: a #GError
 *
 * Check the preconditions of the given transition in the state given by @environment. The @environment will not be modified.
 *
 * If the preconditions are satisfied, %TRUE will be returned; %FALSE will be returned otherwise. If the preconditions are not satisfied and they
 * specified a D-Bus error to be thrown on failure, the error will be set in @error and %FALSE will be returned.
 *
 * Return value: %TRUE if the transition's preconditions are satisfied; %FALSE otherwise
 */
gboolean
dfsm_ast_transition_check_preconditions (DfsmAstTransition *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstTransitionPrivate *priv;
	guint i;

	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), FALSE);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), FALSE);
	g_return_val_if_fail (error != NULL && *error == NULL, FALSE);

	priv = self->priv;

	/* Check each of the preconditions in order and return when the first one fails. */
	for (i = 0; i < priv->preconditions->len; i++) {
		DfsmAstPrecondition *precondition = (DfsmAstPrecondition*) g_ptr_array_index (priv->preconditions, i);

		if (dfsm_ast_precondition_check_is_satisfied (precondition, environment, error) == FALSE) {
			return FALSE;
		}
	}

	g_assert (*error == NULL);

	return TRUE;
}

/**
 * dfsm_ast_transition_execute:
 * @self: a #DfsmAstTransition
 * @environment: the environment to execute the transition in
 * @error: a #GError
 *
 * Execute a given state machine transition. This may modify the @environment. It assumes that dfsm_ast_transition_check_preconditions() has already
 * been called for this transition and @environment and has returned %TRUE. It is an error to call this function otherwise.
 *
 * If the transition is successful (i.e. a D-Bus reply is the result), the parameters of the reply will be returned. If the transition is unsuccessful
 * (i.e. a D-Bus error is thrown) the error will be returned in @error.
 *
 * Return value: (transfer full): reply parameters from the transition, or %NULL
 */
GVariant *
dfsm_ast_transition_execute (DfsmAstTransition *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstTransitionPrivate *priv;
	GVariant *return_value = NULL;
	guint i;

	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);

	priv = self->priv;

	g_debug ("Executing transition from state ‘%s’ to ‘%s’.", priv->from_state_name, priv->to_state_name);

	for (i = 0; i < priv->statements->len; i++) {
		DfsmAstStatement *statement;
		GVariant *_return_value;
		GError *child_error = NULL;

		statement = g_ptr_array_index (priv->statements, i);
		_return_value = dfsm_ast_statement_execute (statement, environment, &child_error);

		g_assert (_return_value == NULL || return_value == NULL);
		g_assert (_return_value == NULL || child_error == NULL);

		if (_return_value != NULL) {
			return_value = _return_value;
		} else if (child_error != NULL) {
			g_propagate_error (error, child_error);
			return NULL;
		}
	}

	g_assert (return_value == NULL || g_variant_is_floating (return_value) == FALSE);

	return return_value;
}

/**
 * dfsm_ast_transition_get_from_state_name:
 * @self: a #DfsmAstTransition
 *
 * TODO
 *
 * Return value: TODO
 */
const gchar *
dfsm_ast_transition_get_from_state_name (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);

	return self->priv->from_state_name;
}

/**
 * dfsm_ast_transition_get_to_state_name:
 * @self: a #DfsmAstTransition
 *
 * TODO
 *
 * Return value: TODO
 */
const gchar *
dfsm_ast_transition_get_to_state_name (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);

	return self->priv->to_state_name;
}

/**
 * dfsm_ast_transition_get_trigger:
 * @self: a #DfsmAstTransition
 *
 * TODO
 *
 * Return value: TODO
 */
DfsmAstTransitionTrigger
dfsm_ast_transition_get_trigger (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), DFSM_AST_TRANSITION_ARBITRARY);

	return self->priv->trigger;
}

/**
 * dfsm_ast_transition_get_trigger_method_name:
 * @self: a #DfsmAstTransition
 *
 * TODO
 *
 * Return value: TODO
 */
const gchar *
dfsm_ast_transition_get_trigger_method_name (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);
	g_return_val_if_fail (self->priv->trigger == DFSM_AST_TRANSITION_METHOD_CALL, NULL);

	return self->priv->trigger_params.method_name;
}
