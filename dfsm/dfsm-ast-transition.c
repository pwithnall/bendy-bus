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

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-ast-precondition.h"
#include "dfsm-ast-statement.h"
#include "dfsm-ast-statement-reply.h"
#include "dfsm-ast-statement-throw.h"
#include "dfsm-ast-transition.h"
#include "dfsm-internal.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"
#include "dfsm-utils.h"

static void dfsm_ast_transition_dispose (GObject *object);
static void dfsm_ast_transition_finalize (GObject *object);
static void dfsm_ast_transition_sanity_check (DfsmAstNode *node);
static void dfsm_ast_transition_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_transition_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstTransitionPrivate {
	DfsmAstTransitionTrigger trigger;
	union {
		gchar *method_name; /* for DFSM_AST_TRANSITION_METHOD_CALL, otherwise NULL */
		gchar *property_name; /* for DFSM_AST_TRANSITION_PROPERTY_SET, otherwise NULL */
	} trigger_params;
	GPtrArray *preconditions; /* array of DfsmAstPreconditions */
	GPtrArray *statements; /* array of DfsmAstStatements */
	DfsmAstStatementReply *reply_statement; /* cache of the DfsmAstStatementReply in ->statements, if it exists */
	DfsmAstStatementThrow *throw_statement; /* cache of the DfsmAstStatementThrow in ->statements, if it exists */
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

	g_clear_object (&priv->throw_statement);
	g_clear_object (&priv->reply_statement);

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
		case DFSM_AST_TRANSITION_PROPERTY_SET:
			g_free (priv->trigger_params.property_name);
			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to free here */
			break;
		default:
			g_assert_not_reached ();
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_transition_parent_class)->finalize (object);
}

static void
dfsm_ast_transition_sanity_check (DfsmAstNode *node)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (node)->priv;
	guint i;

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			g_assert (priv->trigger_params.method_name != NULL);
			break;
		case DFSM_AST_TRANSITION_PROPERTY_SET:
			g_assert (priv->trigger_params.property_name != NULL);
			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	g_assert (priv->preconditions != NULL);

	for (i = 0; i < priv->preconditions->len; i++) {
		g_assert (g_ptr_array_index (priv->preconditions, i) != NULL);
		dfsm_ast_node_sanity_check (DFSM_AST_NODE (g_ptr_array_index (priv->preconditions, i)));
	}

	g_assert (priv->statements != NULL);

	for (i = 0; i < priv->statements->len; i++) {
		g_assert (g_ptr_array_index (priv->statements, i) != NULL);
		dfsm_ast_node_sanity_check (DFSM_AST_NODE (g_ptr_array_index (priv->statements, i)));
	}

	g_assert (priv->throw_statement == NULL || priv->reply_statement == NULL);

	if (priv->throw_statement != NULL) {
		g_assert (DFSM_IS_AST_STATEMENT_THROW (priv->throw_statement));
	}

	if (priv->reply_statement != NULL) {
		g_assert (DFSM_IS_AST_STATEMENT_REPLY (priv->reply_statement));
	}
}

static void
dfsm_ast_transition_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (node)->priv;
	gboolean reply_statement_count = 0, throw_statement_count = 0;
	guint i;

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			if (g_dbus_is_member_name (priv->trigger_params.method_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus method name: %s"),
				             priv->trigger_params.method_name);
				return;
			}

			break;
		case DFSM_AST_TRANSITION_PROPERTY_SET:
			if (g_dbus_is_member_name (priv->trigger_params.property_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus property name: %s"),
				             priv->trigger_params.property_name);
				return;
			}

			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do here */
			break;
		default:
			g_assert_not_reached ();
	}

	/* Check each of our preconditions, and also ensure that we only throw errors from preconditions if we're method-triggered. */
	for (i = 0; i < priv->preconditions->len; i++) {
		DfsmAstPrecondition *precondition;

		precondition = DFSM_AST_PRECONDITION (g_ptr_array_index (priv->preconditions, i));

		dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (precondition), environment, error);

		if (*error != NULL) {
			return;
		}

		/* Have we illegally included an error in the precondition? */
		if (dfsm_ast_precondition_get_error_name (precondition) != NULL) {
			switch (priv->trigger) {
				case DFSM_AST_TRANSITION_METHOD_CALL:
					/* Nothing to do here. */
					break;
				case DFSM_AST_TRANSITION_PROPERTY_SET:
				case DFSM_AST_TRANSITION_ARBITRARY:
					/* Extraneous error in precondition. */
					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             _("Unexpected ‘throwing’ clause on precondition. Preconditions on property-triggered and random "
					               "transitions must not throw errors."));
					return;
				default:
					g_assert_not_reached ();
			}
		}
	}

	/* Check each of our statements. */
	for (i = 0; i < priv->statements->len; i++) {
		DfsmAstStatement *statement;

		statement = DFSM_AST_STATEMENT (g_ptr_array_index (priv->statements, i));

		dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (statement), environment, error);

		if (*error != NULL) {
			return;
		}

		/* What type of statement is it? */
		if (DFSM_IS_AST_STATEMENT_REPLY (statement)) {
			reply_statement_count++;

			g_clear_object (&priv->reply_statement);
			priv->reply_statement = g_object_ref (statement);
		} else if (DFSM_IS_AST_STATEMENT_THROW (statement)) {
			throw_statement_count++;

			g_clear_object (&priv->throw_statement);
			priv->throw_statement = g_object_ref (statement);
		}
	}

	/* Check that:
	 *  • if we're a method-triggered transition, we have exactly one reply or throw statement;
	 *  • if we're a random transition, we have no reply or throw statements; or
	 *  • if we're a property-triggered transition, we have no reply or throw statements. */
	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			if (reply_statement_count == 0 && throw_statement_count == 0) {
				/* No reply or throw statements. */
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					     _("Missing ‘reply’ or ‘throw’ statement in transition. Exactly one must be present in every transition."));
				return;
			} else if (reply_statement_count + throw_statement_count != 1) {
				/* Too many of either statement. */
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					     _("Too many ‘reply’ or ‘throw’ statements in transition. "
					       "Exactly one must be present in every transition."));
				return;
			}

			break;
		case DFSM_AST_TRANSITION_PROPERTY_SET:
		case DFSM_AST_TRANSITION_ARBITRARY:
			if (reply_statement_count != 0 || throw_statement_count != 0) {
				/* Extraneous reply or throw statement. */
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Unexpected ‘reply’ or ‘throw’ statement in transition. None must be present in property-triggered "
				               "and random transitions."));
				return;
			}

			break;
		default:
			g_assert_not_reached ();
	}
}

static void
dfsm_ast_transition_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstTransitionPrivate *priv = DFSM_AST_TRANSITION (node)->priv;
	guint i;
	GDBusMethodInfo *method_info = NULL;

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL: {
			GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;

			interfaces = dfsm_environment_get_interfaces (environment);

			for (i = 0; i < interfaces->len; i++) {
				GDBusInterfaceInfo *interface_info = (GDBusInterfaceInfo*) g_ptr_array_index (interfaces, i);

				method_info = g_dbus_interface_info_lookup_method (interface_info, priv->trigger_params.method_name);

				if (method_info != NULL) {
					/* Found the interface defining method_name. */
					break;
				}
			}

			/* Failed to find a suitable interface? */
			if (method_info == NULL) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Undeclared D-Bus method referenced as a transition trigger: %s"), priv->trigger_params.method_name);
				goto done;
			}

			/* Add the method's parameters to the environment so they're available when checking sub-nodes and the reply statement. */
			if (method_info->in_args != NULL) {
				GDBusArgInfo **arg_infos;

				for (arg_infos = method_info->in_args; *arg_infos != NULL; arg_infos++) {
					GVariantType *parameter_type;

					parameter_type = g_variant_type_new ((*arg_infos)->signature);
					dfsm_environment_set_variable_type (environment, DFSM_VARIABLE_SCOPE_LOCAL, (*arg_infos)->name, parameter_type);
					g_variant_type_free (parameter_type);
				}
			}

			/* Check the type of the reply statement, if one exists. If there are no out args, the statement has to have unit type.
			 * Otherwise, the statement has to be a struct of the required arity.
			 * Note that this has to be done after the parameters have been added to the environmen, as the parameters might be referenced
			 * by the reply statement. */
			if (priv->reply_statement != NULL) {
				GVariantType *actual_out_type, *expected_out_type;

				actual_out_type = dfsm_ast_expression_calculate_type (dfsm_ast_statement_reply_get_expression (priv->reply_statement),
				                                                      environment);
				expected_out_type = dfsm_internal_dbus_arg_info_array_to_variant_type ((const GDBusArgInfo**) method_info->out_args);

				if (g_variant_type_is_subtype_of (actual_out_type, expected_out_type) == FALSE) {
					gchar *actual_out_type_string, *expected_out_type_string;

					actual_out_type_string = g_variant_type_dup_string (actual_out_type);
					expected_out_type_string = g_variant_type_dup_string (expected_out_type);

					g_variant_type_free (expected_out_type);
					g_variant_type_free (actual_out_type);

					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
					             _("Type mismatch between formal and actual parameters to D-Bus reply statement: "
					               "expects type ‘%s’ but received type ‘%s’."), expected_out_type_string, actual_out_type_string);

					g_free (expected_out_type_string);
					g_free (actual_out_type_string);

					goto done;
				}

				g_variant_type_free (expected_out_type);
				g_variant_type_free (actual_out_type);
			}

			break;
		}
		case DFSM_AST_TRANSITION_PROPERTY_SET: {
			GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;
			GDBusPropertyInfo *property_info;
			GVariantType *property_type;

			interfaces = dfsm_environment_get_interfaces (environment);

			for (i = 0; i < interfaces->len; i++) {
				GDBusInterfaceInfo *interface_info = (GDBusInterfaceInfo*) g_ptr_array_index (interfaces, i);

				property_info = g_dbus_interface_info_lookup_property (interface_info, priv->trigger_params.property_name);

				if (property_info != NULL) {
					/* Found the interface defining property_name. */
					break;
				}
			}

			/* Failed to find a suitable interface? */
			if (property_info == NULL) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Undeclared D-Bus property referenced as a transition trigger: %s"), priv->trigger_params.property_name);
				goto done;
			}

			/* Warn if the property isn't writeable. */
			if ((property_info->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE) == 0) {
				g_warning (_("D-Bus property ‘%s’ referenced as a transition trigger is not writeable."),
				           priv->trigger_params.property_name);
			}

			/* Add the special “value” parameter to the environment so it's available when checking sub-nodes. */
			property_type = g_variant_type_new (property_info->signature);
			dfsm_environment_set_variable_type (environment, DFSM_VARIABLE_SCOPE_LOCAL, "value", property_type);
			g_variant_type_free (property_type);

			break;
		}
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
			goto done;
		}
	}

	for (i = 0; i < priv->statements->len; i++) {
		DfsmAstStatement *statement;

		statement = DFSM_AST_STATEMENT (g_ptr_array_index (priv->statements, i));

		dfsm_ast_node_check (DFSM_AST_NODE (statement), environment, error);

		if (*error != NULL) {
			goto done;
		}
	}

done:
	/* Restore the environment if this is a method- or property-triggered transition. */
	if (priv->trigger == DFSM_AST_TRANSITION_METHOD_CALL && method_info != NULL && method_info->in_args != NULL) {
		GDBusArgInfo **arg_infos;

		for (arg_infos = method_info->in_args; *arg_infos != NULL; arg_infos++) {
			dfsm_environment_unset_variable_value (environment, DFSM_VARIABLE_SCOPE_LOCAL, (*arg_infos)->name);
		}
	} else if (priv->trigger == DFSM_AST_TRANSITION_PROPERTY_SET) {
		dfsm_environment_unset_variable_value (environment, DFSM_VARIABLE_SCOPE_LOCAL, "value");
	}
}

/**
 * dfsm_ast_transition_new:
 * @details: details of the transition trigger (such as a method or property name)
 * @precondition: array of #DfsmAstPrecondition<!-- -->s for the transition
 * @statements: array of #DfsmAstStatement<!-- -->s to execute with the transition
 *
 * Create a new #DfsmAstTransition representing a single transition. The state pairs the transition can be applied to are stored in the #DfsmAstObject
 * containing this transition, since the same #DfsmAstTransition could be applied to many different state pairs.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstTransition *
dfsm_ast_transition_new (const DfsmParserTransitionDetails *details, GPtrArray/*<DfsmAstPrecondition>*/ *preconditions,
                         GPtrArray/*<DfsmAstStatement>*/ *statements)
{
	DfsmAstTransition *transition;
	DfsmAstTransitionPrivate *priv;

	g_return_val_if_fail (details != NULL, NULL);
	g_return_val_if_fail (preconditions != NULL, NULL);
	g_return_val_if_fail (statements != NULL, NULL);

	transition = g_object_new (DFSM_TYPE_AST_TRANSITION, NULL);
	priv = transition->priv;

	priv->trigger = details->transition_type;

	switch (priv->trigger) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			priv->trigger_params.method_name = g_strdup (details->str);
			break;
		case DFSM_AST_TRANSITION_PROPERTY_SET:
			priv->trigger_params.property_name = g_strdup (details->str);
			break;
		case DFSM_AST_TRANSITION_ARBITRARY:
			/* Nothing to do. */
			break;
		default:
			g_assert_not_reached ();
	}

	priv->preconditions = g_ptr_array_ref (preconditions);
	priv->statements = g_ptr_array_ref (statements);

	return transition;
}

/**
 * dfsm_ast_transition_get_preconditions:
 * @self: a #DfsmAstTransition
 *
 * Returns an array of the transition's #DfsmAstPrecondition<!-- -->s. The array may be empty, but will never be %NULL.
 *
 * Return value: (transfer none) (element-type DfsmAstPrecondition): array of preconditions
 */
GPtrArray/*<DfsmAstPrecondition>*/ *
dfsm_ast_transition_get_preconditions (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);

	return self->priv->preconditions;
}

/**
 * dfsm_ast_transition_check_preconditions:
 * @self: a #DfsmAstTransition
 * @environment: the environment to execute the transition in
 * @output_sequence: (allow-none): an output sequence to append the precondition error to if necessary
 * @will_throw_error: (allow-none) (out caller-allocates): return location for %TRUE if the transition will throw an error to @output_sequence on
 * precondition failure, %FALSE otherwise
 *
 * Check the preconditions of the given transition in the state given by @environment. The @environment will not be modified.
 *
 * If the preconditions are satisfied, %TRUE will be returned; %FALSE will be returned otherwise. If the preconditions are not satisfied and they
 * specified a D-Bus error to be thrown on failure, the error will be appended to @output_sequence (if @output_sequence is non-%NULL) and %FALSE will
 * be returned. @will_throw_error will always reflect whether precondition failures will modify @output_sequence, so this function may be called with
 * @output_sequence set to %NULL to determine whether precondition failures will cause D-Bus errors.
 *
 * Return value: %TRUE if the transition's preconditions are satisfied; %FALSE otherwise
 */
gboolean
dfsm_ast_transition_check_preconditions (DfsmAstTransition *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence,
                                         gboolean *will_throw_error)
{
	DfsmAstTransitionPrivate *priv;
	guint i;

	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), FALSE);
	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), FALSE);
	g_return_val_if_fail (output_sequence == NULL || DFSM_IS_OUTPUT_SEQUENCE (output_sequence), FALSE);

	priv = self->priv;

	/* Check each of the preconditions in order and return when the first one fails. */
	for (i = 0; i < priv->preconditions->len; i++) {
		DfsmAstPrecondition *precondition = DFSM_AST_PRECONDITION (g_ptr_array_index (priv->preconditions, i));

		if (dfsm_ast_precondition_check_is_satisfied (precondition, environment) == FALSE) {
			/* If called with an output_sequence, will we throw an error? */
			if (will_throw_error != NULL) {
				*will_throw_error = (dfsm_ast_precondition_get_error_name (precondition) != NULL) ? TRUE : FALSE;
			}

			/* If an output sequence has been provided, append an error to it. */
			if (output_sequence != NULL) {
				dfsm_ast_precondition_throw_error (precondition, output_sequence);
			}

			return FALSE;
		}
	}

	if (will_throw_error != NULL) {
		*will_throw_error = FALSE;
	}

	return TRUE;
}

/**
 * dfsm_ast_transition_execute:
 * @self: a #DfsmAstTransition
 * @environment: the environment to execute the transition in
 * @output_sequence: an output sequence to append the transition's effects to
 *
 * Execute a given state machine transition. This may modify the @environment. It assumes that dfsm_ast_transition_check_preconditions() has already
 * been called for this transition and @environment and has returned %TRUE. It is an error to call this function otherwise.
 *
 * Any effects caused by the transition (such as D-Bus signal emissions, method replies and error replies to D-Bus method calls) will be appended to
 * @output_sequence in execution order. The caller may then use dfsm_output_sequence_output() to push the effects out onto the bus.
 *
 * Return value: (transfer full): reply parameters from the transition, or %NULL
 */
void
dfsm_ast_transition_execute (DfsmAstTransition *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence)
{
	DfsmAstTransitionPrivate *priv;
	guint i;

	g_return_if_fail (DFSM_IS_AST_TRANSITION (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (output_sequence));

	priv = self->priv;

	g_debug ("Executing transition %p in environment %p.", self, environment);

	for (i = 0; i < priv->statements->len; i++) {
		DfsmAstStatement *statement;

		statement = g_ptr_array_index (priv->statements, i);
		dfsm_ast_statement_execute (statement, environment, output_sequence);
	}
}

/**
 * dfsm_ast_transition_get_trigger:
 * @self: a #DfsmAstTransition
 *
 * Gets the type of triggering for this transition.
 *
 * Return value: trigger type for the transition
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
 * Gets the name of the D-Bus method triggering this transition when it's called by the client. It is only valid to call this method if
 * dfsm_ast_transition_get_trigger() returns %DFSM_AST_TRANSITION_METHOD_CALL.
 *
 * Return value: name of the D-Bus method triggering this transition
 */
const gchar *
dfsm_ast_transition_get_trigger_method_name (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);
	g_return_val_if_fail (self->priv->trigger == DFSM_AST_TRANSITION_METHOD_CALL, NULL);

	return self->priv->trigger_params.method_name;
}

/**
 * dfsm_ast_transition_get_trigger_property_name:
 * @self: a #DfsmAstTransition
 *
 * Gets the name of the D-Bus property triggering this transition when it's assigned to. It is only valid to call this method if
 * dfsm_ast_transition_get_trigger() returns %DFSM_AST_TRANSITION_PROPERTY_SET.
 *
 * Return value: name of the D-Bus property triggering this transition
 */
const gchar *
dfsm_ast_transition_get_trigger_property_name (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);
	g_return_val_if_fail (self->priv->trigger == DFSM_AST_TRANSITION_PROPERTY_SET, NULL);

	return self->priv->trigger_params.property_name;
}

/**
 * dfsm_ast_transition_contains_throw_statement:
 * @self: a #DfsmAstTransition
 *
 * Gets whether the transition contains a statement of type #DfsmAstStatementThrow. The alternatives are to contain a #DfsmAstStatementReply instead,
 * or to contain neither statement, depending on the trigger type of the transition.
 *
 * It is only valid to call this method after successfully calling dfsm_ast_node_pre_check_and_register().
 *
 * Return value: %TRUE if the transition contains a #DfsmAstStatementThrow, %FALSE otherwise
 */
gboolean
dfsm_ast_transition_contains_throw_statement (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), FALSE);

	return (self->priv->throw_statement != NULL) ? TRUE : FALSE;
}

/**
 * dfsm_ast_transition_get_statements:
 * @self: a #DfsmAstTransition
 *
 * Returns an array of the transition's #DfsmAstStatement<!-- -->s. The array will always contain at least one element, and will never be %NULL.
 *
 * Return value: (transfer none) (element-type DfsmAstStatement): array of statements
 */
GPtrArray/*<DfsmAstStatement>*/ *
dfsm_ast_transition_get_statements (DfsmAstTransition *self)
{
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (self), NULL);

	g_assert (self->priv->statements->len > 0);
	return self->priv->statements;
}
