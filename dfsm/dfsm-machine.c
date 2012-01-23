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

#include "dfsm-ast.h"
#include "dfsm-environment.h"
#include "dfsm-machine.h"
#include "dfsm-marshal.h"
#include "dfsm-output-sequence.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"
#include "dfsm-probabilities.h"

static void dfsm_machine_dispose (GObject *object);
static void dfsm_machine_get_gobject_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dfsm_machine_set_gobject_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _DfsmMachinePrivate {
	/* Simulation data */
	DfsmMachineStateNumber machine_state;
	DfsmEnvironment *environment;

	/* Static data */
	GPtrArray/*<string>*/ *state_names; /* (indexed by DfsmMachineStateNumber) */
	struct {
		GHashTable/*<string, GPtrArray<DfsmAstObjectTransition>>*/ *method_call_triggered; /* hash table of method name to transitions */
		GHashTable/*<string, GPtrArray<DfsmAstObjectTransition>>*/ *property_set_triggered; /* hash table of property name to transitions */
		GPtrArray/*<DfsmAstObjectTransition>*/ *arbitrarily_triggered; /* array of transitions */
	} transitions;
};

enum {
	PROP_MACHINE_STATE = 1,
	PROP_ENVIRONMENT,
};

G_DEFINE_TYPE (DfsmMachine, dfsm_machine, G_TYPE_OBJECT)

static void
dfsm_machine_class_init (DfsmMachineClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmMachinePrivate));

	gobject_class->get_property = dfsm_machine_get_gobject_property;
	gobject_class->set_property = dfsm_machine_set_gobject_property;
	gobject_class->dispose = dfsm_machine_dispose;

	/**
	 * DfsmMachine:machine-state:
	 *
	 * The index of the current state of the machine. This will always be valid.
	 */
	g_object_class_install_property (gobject_class, PROP_MACHINE_STATE,
	                                 g_param_spec_uint ("machine-state",
	                                                    "Machine state", "The index of the current state of the machine.",
	                                                    0, G_MAXUINT, DFSM_MACHINE_STARTING_STATE,
	                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * DfsmMachine:environment:
	 *
	 * The execution environment this machine simulation is using.
	 */
	g_object_class_install_property (gobject_class, PROP_ENVIRONMENT,
	                                 g_param_spec_object ("environment",
	                                                      "Environment", "The execution environment this machine simulation is using.",
	                                                      DFSM_TYPE_ENVIRONMENT,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
dfsm_machine_init (DfsmMachine *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_MACHINE, DfsmMachinePrivate);
	self->priv->machine_state = DFSM_MACHINE_STARTING_STATE;
}

static void
dfsm_machine_dispose (GObject *object)
{
	DfsmMachinePrivate *priv = DFSM_MACHINE (object)->priv;

	/* Free things. */
	if (priv->environment != NULL) {
		g_object_unref (priv->environment);
		priv->environment = NULL;
	}

	if (priv->state_names != NULL) {
		g_ptr_array_unref (priv->state_names);
		priv->state_names = NULL;
	}

	if (priv->transitions.method_call_triggered != NULL) {
		g_hash_table_unref (priv->transitions.method_call_triggered);
		priv->transitions.method_call_triggered = NULL;
	}

	if (priv->transitions.property_set_triggered != NULL) {
		g_hash_table_unref (priv->transitions.property_set_triggered);
		priv->transitions.property_set_triggered = NULL;
	}

	if (priv->transitions.arbitrarily_triggered != NULL) {
		g_ptr_array_unref (priv->transitions.arbitrarily_triggered);
		priv->transitions.arbitrarily_triggered = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_machine_parent_class)->dispose (object);
}

static void
dfsm_machine_get_gobject_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DfsmMachinePrivate *priv = DFSM_MACHINE (object)->priv;

	switch (property_id) {
		case PROP_MACHINE_STATE:
			g_value_set_uint (value, priv->machine_state);
			break;
		case PROP_ENVIRONMENT:
			g_value_set_object (value, priv->environment);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dfsm_machine_set_gobject_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DfsmMachinePrivate *priv = DFSM_MACHINE (object)->priv;

	switch (property_id) {
		case PROP_ENVIRONMENT:
			/* Construct-only */
			priv->environment = g_value_dup_object (value);
			dfsm_environment_save_reset_point (priv->environment);
			break;
		case PROP_MACHINE_STATE:
			/* Read-only */
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static const gchar *
get_state_name (DfsmMachine *self, DfsmMachineStateNumber state_number)
{
	return (const gchar*) g_ptr_array_index (self->priv->state_names, state_number);
}

/* Return value: whether the machine changed state */
static gboolean
execute_transition (DfsmMachine *self, DfsmAstObjectTransition *object_transition, DfsmOutputSequence *output_sequence)
{
	DfsmMachinePrivate *priv = self->priv;

	g_debug ("…Executing transition %p from ‘%s’ to ‘%s’.", object_transition->transition, get_state_name (self, object_transition->from_state),
	         get_state_name (self, object_transition->to_state));
	dfsm_ast_transition_execute (object_transition->transition, priv->environment, output_sequence);

	/* Various possibilities for return values. */
	if (dfsm_ast_transition_contains_throw_statement (object_transition->transition) == FALSE) {
		/* Success, with or without a return value. */
		g_debug ("…(Successful.)");

		/* Change machine state. */
		priv->machine_state = object_transition->to_state;
		g_object_notify (G_OBJECT (self), "machine-state");

		return TRUE;
	} else {
		/* A ‘throw’ statement was executed. Don't change states. */
		g_debug ("…(Threw error.)");

		return FALSE;
	}
}

static gboolean
find_and_execute_random_transition (DfsmMachine *self, DfsmOutputSequence *output_sequence, GPtrArray/*<DfsmAstObjectTransition>*/ *possible_transitions)
{
	DfsmMachinePrivate *priv = self->priv;
	guint i, rand_offset;
	DfsmAstObjectTransition *candidate_object_transition = NULL, *precondition_failure_transition = NULL;
	gboolean outputted = FALSE; /* have we outputted a reply or thrown an error? */

	g_debug ("Finding a transition out of %u possibles.", possible_transitions->len);

	/* If there are no possible transitions, bail out. */
	if (possible_transitions->len == 0) {
		g_debug ("…No possible transitions.");
		goto done;
	}

	/* Arbitrarily choose a transition to perform. We do this by taking a random start index into the array of transitions, and then sequentially
	 * checking preconditions of transitions until we find one which is satisfied. We then execute that transition.
	 *
	 * We weight the probability of executing a given transition by the type of transition. If the transition contains a ‘throw’ statement then
	 * we have a 0.8 chance of skipping the transition. This is an attempt to make the simulation follow ‘interesting’ transitions more often
	 * than not.
	 *
	 * If we're trying to find a transition as a result of a method call or property change, we can try to prioritise non-throwing transitions over
	 * those which contain ‘throw’ statements; but we *must* pick a transition. We can't just ignore the method call/property change.
	 *
	 * If we're trying to find a transition as a result of a random timeout, we can prioritise non-throwing transitions over those which contain
	 * ‘throw’ statements to the extent that we may not actually execute a transition. That's fine. */
	rand_offset = g_random_int_range (0, possible_transitions->len);
	for (i = 0; i < possible_transitions->len; i++) {
		DfsmAstObjectTransition *object_transition;
		DfsmAstTransition *transition;
		gboolean will_throw_error = FALSE;

		object_transition = g_ptr_array_index (possible_transitions, (i + rand_offset) % possible_transitions->len);
		transition = object_transition->transition;

		/* Check we're in the right starting state. */
		if (object_transition->from_state != priv->machine_state) {
			g_debug ("…Skipping transition %p from ‘%s’ to ‘%s’ due to being in the wrong state (‘%s’).", transition,
			         get_state_name (self, object_transition->from_state), get_state_name (self, object_transition->to_state),
			         get_state_name (self, priv->machine_state));
			continue;
		}

		/* If this transition's preconditions are satisfied, continue down to execute it. Otherwise, loop round and try the next transition. */
		if (dfsm_ast_transition_check_preconditions (transition, priv->environment, NULL, &will_throw_error) == FALSE) {
			/* If the transition will throw a D-Bus error as a result of its precondition failures, store it. If we don't find any
			 * transitions which have no precondition failures, we can come back to the first one _with_ precondition failures and
			 * throw its D-Bus errors. */
			if (precondition_failure_transition == NULL && will_throw_error == TRUE) {
				precondition_failure_transition = object_transition;
			}

			g_debug ("…Skipping transition %p from ‘%s’ to ‘%s’ due to precondition failures.", transition,
			         get_state_name (self, object_transition->from_state), get_state_name (self, object_transition->to_state));

			continue;
		}

		/* If this transition contains a ‘throw’ statement, check if we really want to execute it. */
		if (dfsm_ast_transition_contains_throw_statement (transition) == TRUE && DFSM_BIASED_COIN_FLIP (0.8)) {
			/* Skip the transition, but keep a record of it in case we find there are no other transitions whose preconditions pass and
			 * which don't contain ‘throw’ statements. */
			candidate_object_transition = object_transition;
			precondition_failure_transition = NULL;

			g_debug ("…Skipping transition %p from ‘%s’ to ‘%s’ due to it containing a throw statement.", transition,
			         get_state_name (self, object_transition->from_state), get_state_name (self, object_transition->to_state));

			continue;
		}

		/* We're found a transition whose preconditions pass, so forget about any previous precondition failures. Execute the transition. */
		precondition_failure_transition = NULL;
		candidate_object_transition = NULL;

		execute_transition (self, object_transition, output_sequence);
		outputted = TRUE;

		break;
	}

	/* If we didn't manage to find/execute any transitions, return the error from the first precondition failure. */
	if (precondition_failure_transition != NULL) {
		dfsm_ast_transition_check_preconditions (precondition_failure_transition->transition, priv->environment, output_sequence, NULL);
		outputted = TRUE;
	}

	/* If we found a candidate transition but then skipped it due to it containing ‘throw’ statements, and didn't subsequently find a better
	 * transition, execute the previous candidate transition now. */
	if (candidate_object_transition != NULL) {
		execute_transition (self, candidate_object_transition, output_sequence);
		outputted = TRUE;
	}

done:
	g_assert (precondition_failure_transition == NULL || candidate_object_transition == NULL);
	g_assert (outputted == TRUE || (precondition_failure_transition == NULL && candidate_object_transition == NULL));

	return outputted;
}

/**
 * dfsm_machine_make_arbitrary_transition:
 * @self: a #DfsmMachine
 * @output_sequence: an output sequence to append effects of the transition to
 *
 * Make an arbitrary (i.e. triggered randomly) transition in the machine's state, if one is available to be taken at the moment (e.g. its preconditions
 * are met). This should typically be called on a timer.
 *
 * If a transition is taken, any effects of the transition's code will be appended to @output_sequence in execution order. The caller may then push
 * those effects out onto the bus using dfsm_output_sequence_output().
 */
void
dfsm_machine_make_arbitrary_transition (DfsmMachine *self, DfsmOutputSequence *output_sequence)
{
	DfsmMachinePrivate *priv;
	GPtrArray/*<DfsmAstTransition>*/ *possible_transitions;
	gboolean executed_transition = FALSE;

	g_return_if_fail (DFSM_IS_MACHINE (self));
	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (output_sequence));

	priv = self->priv;

	/* At this point, all arbitrary transitions are possible */
	possible_transitions = priv->transitions.arbitrarily_triggered;

	/* Find and potentially execute a transition. */
	executed_transition = find_and_execute_random_transition (self, output_sequence, possible_transitions);

	if (executed_transition == FALSE) {
		/* If we failed to execute a transition, log it and ignore it. */
		g_debug ("Couldn't find any arbitrary DFSM transitions eligible to be executed as a result of a timeout. Ignoring.");
	} else {
		/* Success! */
		g_debug ("Successfully executed an arbitrary DFSM transition as a result of a timeout.");
	}
}

/*
 * dfsm_machine_new:
 * @environment: a #DfsmEnvironment containing all the variables and functions used by the machine
 * @state_names: an array of strings of all the state names used in the DFSM
 * @transitions: an array of structures (#DfsmAstObjectTransition<!-- -->s) representing each of the possible transitions in the DFSM
 *
 * Creates a new #DfsmMachine with the given environment, states and set of transitions.
 *
 * Return value: (transfer full): a new #DfsmMachine
 */
DfsmMachine *
_dfsm_machine_new (DfsmEnvironment *environment, GPtrArray/*<string>*/ *state_names, GPtrArray/*<DfsmAstObjectTransition>*/ *transitions)
{
	DfsmMachine *machine;
	guint i;

	g_return_val_if_fail (DFSM_IS_ENVIRONMENT (environment), NULL);

	machine = g_object_new (DFSM_TYPE_MACHINE,
	                        "environment", environment,
	                        NULL);

	/* States */
	machine->priv->state_names = g_ptr_array_ref (state_names);

	/* Transitions need to be sorted by trigger method */
	machine->priv->transitions.method_call_triggered = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
	machine->priv->transitions.property_set_triggered = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
	machine->priv->transitions.arbitrarily_triggered = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_ast_object_transition_unref);

	for (i = 0; i < transitions->len; i++) {
		DfsmAstObjectTransition *object_transition;
		DfsmAstTransition *transition;

		object_transition = g_ptr_array_index (transitions, i);
		transition = object_transition->transition;

		switch (dfsm_ast_transition_get_trigger (transition)) {
			case DFSM_AST_TRANSITION_METHOD_CALL: {
				GPtrArray/*<DfsmAstObjectTransition>*/ *new_transitions;

				/* Add the transition to a new or existing array of transitions for the given method name. */
				new_transitions = g_hash_table_lookup (machine->priv->transitions.method_call_triggered,
				                                       dfsm_ast_transition_get_trigger_method_name (transition));

				if (new_transitions == NULL) {
					new_transitions = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_ast_object_transition_unref);
					g_hash_table_insert (machine->priv->transitions.method_call_triggered,
					                     g_strdup (dfsm_ast_transition_get_trigger_method_name (transition)), new_transitions);
				}

				g_ptr_array_add (new_transitions, dfsm_ast_object_transition_ref (object_transition));

				break;
			}
			case DFSM_AST_TRANSITION_PROPERTY_SET: {
				GPtrArray/*<DfsmAstObjectTransition>*/ *new_transitions;

				/* Add the transition to a new or existing array of transitions for the given property name. */
				new_transitions = g_hash_table_lookup (machine->priv->transitions.property_set_triggered,
				                                       dfsm_ast_transition_get_trigger_property_name (transition));

				if (new_transitions == NULL) {
					new_transitions = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_ast_object_transition_unref);
					g_hash_table_insert (machine->priv->transitions.property_set_triggered,
					                     g_strdup (dfsm_ast_transition_get_trigger_property_name (transition)), new_transitions);
				}

				g_ptr_array_add (new_transitions, dfsm_ast_object_transition_ref (object_transition));

				break;
			}
			case DFSM_AST_TRANSITION_ARBITRARY:
				/* Arbitrary transition */
				g_ptr_array_add (machine->priv->transitions.arbitrarily_triggered, dfsm_ast_object_transition_ref (object_transition));
				break;
			default:
				g_assert_not_reached ();
		}
	}

	return machine;
}

/**
 * dfsm_machine_reset_simulation:
 * @self: a #DfsmMachine
 *
 * Reset the simulation's state. This can be called whether the simulation is currently running or stopped. In both cases, it resets the DFSM to its
 * starting state and resets the environment.
 */
void
dfsm_machine_reset_state (DfsmMachine *self)
{
	DfsmMachinePrivate *priv;

	g_return_if_fail (DFSM_IS_MACHINE (self));

	priv = self->priv;

	g_debug ("Resetting the simulation.");

	/* Reset the machine state. */
	priv->machine_state = DFSM_MACHINE_STARTING_STATE;
	g_object_notify (G_OBJECT (self), "machine-state");

	/* Reset the environment. */
	dfsm_environment_reset (priv->environment);
}

/**
 * dfsm_machine_call_method:
 * @self: a #DfsmMachine
 * @output_sequence: output sequence for results and effects of the method
 * @interface_name: the name of the D-Bus interface that @method_name is defined on
 * @method_name: the name of the D-Bus method to call
 * @parameters: parameters for the D-Bus method
 *
 * Call the given @method_name on the DFSM machine with the given @parameters as input. The method will be called synchronously, and its effects added
 * to the given @output_sequence, including its return value (if any). If the method throws a D-Bus error, that will be set in @output_sequence.
 *
 * Any signal emissions and property changes which occur as a result of the method call will be added to the @output_sequence.
 */
void
dfsm_machine_call_method (DfsmMachine *self, DfsmOutputSequence *output_sequence, const gchar *interface_name, const gchar *method_name,
                          GVariant *parameters)
{
	DfsmMachinePrivate *priv;
	GPtrArray/*<DfsmAstObjectTransition>*/ *possible_transitions;
	gboolean executed_transition = FALSE;
	GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;
	GDBusInterfaceInfo *interface_info = NULL;
	GDBusMethodInfo *method_info;
	guint i;

	g_return_if_fail (DFSM_IS_MACHINE (self));
	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (output_sequence));
	g_return_if_fail (interface_name != NULL && *interface_name != '\0');
	g_return_if_fail (method_name != NULL && *method_name != '\0');
	g_return_if_fail (parameters != NULL);

	priv = self->priv;

	/* Look up the method name in our set of transitions which are triggered by method calls */
	possible_transitions = g_hash_table_lookup (priv->transitions.method_call_triggered, method_name);

	if (possible_transitions == NULL || possible_transitions->len == 0) {
		/* Unknown method call. Spit out a warning and then return the unit tuple. If this is of the wrong type, then tough. We don't want
		 * to start trying to make up arbitrary data structures to match a given method return type. */
		g_warning (_("Unrecognized method call to ‘%s’ on DFSM. Ignoring method call."), method_name);
		goto done;
	}

	/* Add the method's in parameters to the environment. */
	interfaces = dfsm_environment_get_interfaces (priv->environment);

	for (i = 0; i < interfaces->len; i++) {
		if (strcmp (interface_name, ((GDBusInterfaceInfo*) g_ptr_array_index (interfaces, i))->name) == 0) {
			interface_info = g_ptr_array_index (interfaces, i);
			break;
		}
	}

	if (interface_info == NULL) {
		g_warning (_("Runtime error in simulation: Couldn't find interface containing method ‘%s’."), method_name);
		goto done;
	}

	method_info = g_dbus_interface_info_lookup_method (interface_info, method_name);

	if (method_info == NULL) {
		g_warning (_("Runtime error in simulation: Couldn't find interface containing method ‘%s’."), method_name);
		goto done;
	}

	/* Add the parameters to the environment. */
	if (method_info->in_args != NULL) {
		for (i = 0; method_info->in_args[i] != NULL && i < g_variant_n_children (parameters); i++) {
			GVariant *parameter;
			GVariantType *parameter_type;

			/* Add the (i)th tuple child of the input parameters to the environment with the name given by the (i)th in argument in the
			 * method info. */
			parameter = g_variant_get_child_value (parameters, i);
			parameter_type = g_variant_type_new (method_info->in_args[i]->signature);

			dfsm_environment_set_variable_type (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, method_info->in_args[i]->name, parameter_type);
			dfsm_environment_set_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, method_info->in_args[i]->name, parameter);

			g_variant_type_free (parameter_type);
			g_variant_unref (parameter);
		}
	}

	if (method_info->in_args[i] != NULL || i < g_variant_n_children (parameters)) {
		g_warning (_("Runtime error in simulation: mismatch between interface and input of in-args for method ‘%s’. Continuing."), method_name);
	}

	/* Find and potentially execute a transition from the array. */
	executed_transition = find_and_execute_random_transition (self, output_sequence, possible_transitions);

	/* Restore the environment. */
	if (method_info->in_args != NULL) {
		for (i = 0; method_info->in_args[i] != NULL && i < g_variant_n_children (parameters); i++) {
			/* Remove the variable with the name given by the (i)th in argument in the method info from the environment. */
			dfsm_environment_unset_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, method_info->in_args[i]->name);
		}
	}

done:
	/* If we failed to find and execute a transition, warn and return the unit tuple. */
	if (executed_transition == FALSE) {
		GVariant *return_value;

		g_warning (_("Failed to execute any DFSM transitions as a result of method call ‘%s’. Ignoring method call."), method_name);

		return_value = g_variant_ref_sink (g_variant_new_tuple (NULL, 0));
		dfsm_output_sequence_add_reply (output_sequence, return_value);
		g_variant_unref (return_value);
	}
}

/**
 * dfsm_machine_set_property:
 * @self: a #DfsmMachine
 * @output_sequence: an output sequence to append effects of setting the property to
 * @interface_name: the name of the D-Bus interface that @property_name is defined on
 * @method_name: the name of the D-Bus property to set
 * @value: new value for the property
 *
 * Set the given @property_name to @value on the DFSM machine. The property will be set synchronously.
 *
 * Any signal emissions which occur as a result of the property being set will be appended to @output_sequence. Any additional property changes will
 * be made while the function's running (but signalled in @output_sequence, if anywhere).
 *
 * Return value: %TRUE if @property_name was changed (not just set) to @value, %FALSE otherwise
 */
gboolean
dfsm_machine_set_property (DfsmMachine *self, DfsmOutputSequence *output_sequence, const gchar *interface_name, const gchar *property_name,
                           GVariant *value)
{
	DfsmMachinePrivate *priv;
	GPtrArray/*<DfsmAstObjectTransition>*/ *possible_transitions;
	gboolean executed_transition = FALSE;
	GVariant *return_value = NULL;

	g_return_val_if_fail (DFSM_IS_MACHINE (self), FALSE);
	g_return_val_if_fail (DFSM_IS_OUTPUT_SEQUENCE (output_sequence), FALSE);
	g_return_val_if_fail (interface_name != NULL && *interface_name != '\0', FALSE);
	g_return_val_if_fail (property_name != NULL && *property_name != '\0', FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = self->priv;

	/* Look up the property name in our set of transitions which are triggered by property setters. */
	possible_transitions = g_hash_table_lookup (priv->transitions.property_set_triggered, property_name);

	if (possible_transitions == NULL || possible_transitions->len == 0) {
		/* Unknown property. Run the default transition below. */
		goto done;
	}

	/* Add the property value to the environment. */
	dfsm_environment_set_variable_type (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, "value", g_variant_get_type (value));
	dfsm_environment_set_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, "value", value);

	/* Find and potentially execute a transition from the array. */
	executed_transition = find_and_execute_random_transition (self, output_sequence, possible_transitions);

	/* Restore the environment. */
	dfsm_environment_unset_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, "value");

	/* Swallow the return value. */
	if (return_value != NULL) {
		g_variant_unref (return_value);
	}

done:
	/* If we failed to execute a transition, run a default transition which just sets the corresponding variable in the object. We don't do this if
	 * a transition was found and run successfully. */
	if (executed_transition == FALSE) {
		GVariant *old_value;

		g_debug ("Couldn't find any DFSM transitions eligible to be executed as a result of setting property ‘%s’. Running default transition.",
		         property_name);

		/* Check to see if the value's actually changed. If it hasn't, bail. */
		old_value = dfsm_environment_dup_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, property_name);

		if (old_value != NULL && g_variant_equal (old_value, value) == TRUE) {
			g_variant_unref (old_value);
			return FALSE;
		}

		g_variant_unref (old_value);

		/* Set the variable's new value in the environment. */
		dfsm_environment_set_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, property_name, value);

		return TRUE;
	}

	/* Success! */
	return TRUE;
}

/**
 * dfsm_machine_get_environment:
 * @self: a #DfsmMachine
 *
 * Gets the #DfsmEnvironment which the simulation is running/will run inside.
 *
 * Return value: (transfer none): the machine's environment
 */
DfsmEnvironment *
dfsm_machine_get_environment (DfsmMachine *self)
{
	g_return_val_if_fail (DFSM_IS_MACHINE (self), NULL);

	return self->priv->environment;
}
