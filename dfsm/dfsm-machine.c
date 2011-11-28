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

#include "dfsm-ast.h"
#include "dfsm-environment.h"
#include "dfsm-machine.h"
#include "dfsm-marshal.h"
#include "dfsm-parser.h"

GQuark
dfsm_simulation_error_quark (void)
{
	return g_quark_from_static_string ("dfsm-simulation-error-quark");
}

GType
dfsm_simulation_status_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ DFSM_SIMULATION_STATUS_STOPPED, "DFSM_SIMULATION_STATUS_STOPPED", "stopped" },
			{ DFSM_SIMULATION_STATUS_STARTED, "DFSM_SIMULATION_STATUS_STARTED", "started" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("DfsmSimulationStatus", values);
	}

	return etype;
}

/* Arbitrarily-chosen min. and max. values for the arbitrary transition timeout callbacks. */
#define MIN_TIMEOUT 500 /* ms */
#define MAX_TIMEOUT 5000 /* ms */

static void dfsm_machine_dispose (GObject *object);
static void dfsm_machine_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dfsm_machine_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void environment_signal_emission_cb (DfsmEnvironment *environment, const gchar *signal_name, GVariant *parameters, DfsmMachine *self);

struct _DfsmMachinePrivate {
	/* Simulation data */
	DfsmMachineStateNumber machine_state;
	DfsmEnvironment *environment;
	gulong signal_emission_handler;
	DfsmSimulationStatus simulation_status;
	guint timeout_id;

	/* Static data */
	GPtrArray/*<string>*/ *state_names; /* (indexed by DfsmMachineStateNumber) */
	struct {
		GHashTable/*<string, GPtrArray<DfsmAstTransition>>*/ *method_call_triggered; /* hash table of method name to transitions */
		GPtrArray/*<DfsmAstTransition>*/ *arbitrarily_triggered; /* array of transitions */
	} transitions;
};

enum {
	PROP_MACHINE_STATE = 1,
	PROP_SIMULATION_STATUS,
	PROP_ENVIRONMENT,
};

enum {
	SIGNAL_SIGNAL_EMISSION,
	LAST_SIGNAL,
};

static guint machine_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (DfsmMachine, dfsm_machine, G_TYPE_OBJECT)

static void
dfsm_machine_class_init (DfsmMachineClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmMachinePrivate));

	gobject_class->get_property = dfsm_machine_get_property;
	gobject_class->set_property = dfsm_machine_set_property;
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
	 * DfsmMachine:simulation-status:
	 *
	 * The status of the simulation; i.e. whether it's currently started or stopped.
	 */
	g_object_class_install_property (gobject_class, PROP_SIMULATION_STATUS,
	                                 g_param_spec_enum ("simulation-status",
	                                                    "Simulation status", "The status of the simulation.",
	                                                    DFSM_TYPE_SIMULATION_STATUS, DFSM_SIMULATION_STATUS_STOPPED,
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

	/**
	 * DfsmMachine::signal-emission:
	 * @parameters: the non-floating parameter (or structure of parameters) passed to the signal emission
	 *
	 * Emitted whenever a piece of code in a simulated DFSM emits a D-Bus signal. No code in the simulator will actually emit this D-Bus signal on
	 * a bus instance, but (for example) a wrapper which was listening to this signal could do so.
	 *
	 * This is just a pass-through of #DfsmEnvironment::signal-emission.
	 */
	machine_signals[SIGNAL_SIGNAL_EMISSION] = g_signal_new ("signal-emission",
	                                                        G_TYPE_FROM_CLASS (klass),
	                                                        G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
	                                                        0, NULL, NULL,
	                                                        dfsm_marshal_VOID__STRING_VARIANT,
	                                                        G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VARIANT);
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

	/* Stop the simulation first. */
	dfsm_machine_stop_simulation (DFSM_MACHINE (object));

	/* Free things. */
	if (priv->environment != NULL) {
		g_signal_handler_disconnect (priv->environment, priv->signal_emission_handler);
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

	if (priv->transitions.arbitrarily_triggered != NULL) {
		g_ptr_array_unref (priv->transitions.arbitrarily_triggered);
		priv->transitions.arbitrarily_triggered = NULL;
	}

	/* Make sure we're not leaking a callback. */
	g_assert (priv->timeout_id == 0);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_machine_parent_class)->dispose (object);
}

static void
dfsm_machine_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DfsmMachinePrivate *priv = DFSM_MACHINE (object)->priv;

	switch (property_id) {
		case PROP_MACHINE_STATE:
			g_value_set_uint (value, priv->machine_state);
			break;
		case PROP_SIMULATION_STATUS:
			g_value_set_enum (value, priv->simulation_status);
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
dfsm_machine_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DfsmMachinePrivate *priv = DFSM_MACHINE (object)->priv;

	switch (property_id) {
		case PROP_ENVIRONMENT:
			/* Construct-only */
			priv->environment = g_value_dup_object (value);
			priv->signal_emission_handler = g_signal_connect (priv->environment, "signal-emission",
			                                                  (GCallback) environment_signal_emission_cb, object);
			break;
		case PROP_MACHINE_STATE:
			/* Read-only */
		case PROP_SIMULATION_STATUS:
			/* Read-only */
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static DfsmMachineStateNumber
get_state_number_from_name (DfsmMachine *self, const gchar *state_name)
{
	guint i;

	/* TODO: This is nasty and slow. Can't we eliminate state names entirely by this point? */
	for (i = 0; i < self->priv->state_names->len; i++) {
		const gchar *candidate_name;

		candidate_name = g_ptr_array_index (self->priv->state_names, i);

		if (strcmp (candidate_name, state_name) == 0) {
			return i;
		}
	}

	g_assert_not_reached ();
}

static GVariant *
find_and_execute_random_transition (DfsmMachine *self, GPtrArray/*<DfsmAstTransition>*/ *possible_transitions, gboolean *executed_transition,
                                    GError **error)
{
	DfsmMachinePrivate *priv = self->priv;
	guint i, rand_offset;
	GVariant *return_value = NULL;
	GError *child_error = NULL, *precondition_error = NULL;

	g_assert (executed_transition != NULL);
	*executed_transition = FALSE;

	g_debug ("Finding a transition out of %u possibles.", possible_transitions->len);

	/* If there are no possible transitions, bail out. */
	if (possible_transitions->len == 0) {
		g_debug ("…No possible transitions.");
		goto done;
	}

	/* Arbitrarily choose a transition to perform. We do this by taking a random start index into the array of transitions, and then sequentially
	 * checking preconditions of transitions until we find one which is satisfied. We then execute that transition. */
	rand_offset = g_random_int_range (0, possible_transitions->len);
	for (i = 0; i < possible_transitions->len; i++) {
		DfsmAstTransition *transition;

		transition = g_ptr_array_index (possible_transitions, (i + rand_offset) % possible_transitions->len);

		/* Check we're in the right starting state. */
		if (get_state_number_from_name (self, transition->from_state_name) != priv->machine_state) {
			g_debug ("…Skipping transition %p due to being in the wrong state.", transition);
			continue;
		}

		/* If this transition's preconditions are satisfied, execute it. Otherwise, loop round and try the next transition. */
		if (dfsm_ast_transition_check_preconditions (transition, priv->environment, &child_error) == FALSE) {
			g_debug ("…Skipping transition %p due to precondition failures.", transition);

			/* Errors? These will either be runtime errors in evaluating the condition, or (more likely) errors thrown as a result of
			 * precondition failure. In either case, we store the first occurrence and loop round to try the next transition instead. */
			if (child_error != NULL) {
				if (precondition_error == NULL) {
					precondition_error = child_error;
					child_error = NULL;
				}

				g_clear_error (&child_error);
			}

			continue;
		}

		/* We're found a transition whose preconditions pass, so forget about any previous precondition failures. */
		g_clear_error (&precondition_error);

		/* Execute the transition. */
		g_debug ("…Executing transition %p.", transition);
		return_value = dfsm_ast_transition_execute (transition, priv->environment, &child_error);

		/* Various possibilities for return values. */
		if (child_error == NULL) {
			gchar *return_value_string;

			/* Success, with or without a return value. */
			return_value_string = (return_value != NULL) ? g_variant_print (return_value, TRUE) : g_strdup ("(null)");
			g_debug ("…(Successful, with return value: %s.)", return_value_string);
			g_free (return_value_string);

			*executed_transition = TRUE;

			/* Change machine state. */
			priv->machine_state = get_state_number_from_name (self, transition->to_state_name);
			g_object_notify (G_OBJECT (self), "machine-state");
		} else if (return_value == NULL && child_error != NULL) {
			/* Error, either during execution or as a result of a throw statement. Don't change states. */
			g_debug ("…(Error: %s)", child_error->message);

			g_propagate_error (error, child_error);
			*executed_transition = FALSE;
		} else {
			g_assert_not_reached ();
		}

		break;
	}

	/* If we didn't manage to execute any transitions, return the error from the first precondition failure. */
	if (precondition_error != NULL) {
		child_error = precondition_error;
		precondition_error = NULL;
	}

done:
	g_assert ((*executed_transition == TRUE && child_error == NULL) ||
	          (*executed_transition == FALSE && return_value == NULL));

	return return_value;
}

static void schedule_arbitrary_transition (DfsmMachine *self);

/* This gets called continuously at random intervals while the simulation's running. It checks whether any arbitrary transitions can be taken,
 * and if so, follows one of them. */
static gboolean
arbitrary_transition_timeout_cb (DfsmMachine *self)
{
	DfsmMachinePrivate *priv;
	GPtrArray/*<DfsmAstTransition>*/ *possible_transitions;
	gboolean executed_transition = FALSE;
	GVariant *return_value = NULL;
	GError *child_error = NULL;

	g_return_val_if_fail (DFSM_IS_MACHINE (self), FALSE);

	priv = self->priv;

	g_assert (priv->simulation_status == DFSM_SIMULATION_STATUS_STARTED);

	/* At this point, all arbitrary transitions are possible */
	possible_transitions = priv->transitions.arbitrarily_triggered;

	/* Find and potentially execute a transition. */
	return_value = find_and_execute_random_transition (self, possible_transitions, &executed_transition, &child_error);

	if (executed_transition == FALSE && child_error == NULL) {
		/* If we failed to execute a transition, log it and ignore it. */
		g_debug ("Couldn't find any arbitrary DFSM transitions eligible to be executed as a result of a timeout. Ignoring.");
	} else if (executed_transition == TRUE) {
		/* Success! */
		g_debug ("Successfully executed an arbitrary DFSM transition as a result of a timeout.");

		/* Swallow the result. */
		if (return_value != NULL) {
			g_variant_unref (return_value);
		}
	} else {
		/* Error or precondition failure */
		g_debug ("Error executing an arbitrary DFSM transition as a result of a timeout: %s", child_error->message);
		g_error_free (child_error);
	}

	/* Schedule the next arbitrary transition. */
	priv->timeout_id = 0;
	schedule_arbitrary_transition (self);

	return FALSE;
}

static void
schedule_arbitrary_transition (DfsmMachine *self)
{
	guint32 timeout_period;

	g_assert (self->priv->timeout_id == 0);

	/* Add a random timeout to the next potential arbitrary transition. */
	timeout_period = g_random_int_range (MIN_TIMEOUT, MAX_TIMEOUT);
	g_debug ("Scheduling the next arbitrary transition in %u ms.", timeout_period);
	self->priv->timeout_id = g_timeout_add (timeout_period, (GSourceFunc) arbitrary_transition_timeout_cb, self);
}

static void
environment_signal_emission_cb (DfsmEnvironment *environment, const gchar *signal_name, GVariant *parameters, DfsmMachine *self)
{
	/* Re-emit the signal as a pass-through */
	g_assert (parameters != NULL && g_variant_is_floating (parameters) == FALSE);
	g_signal_emit (self, machine_signals[SIGNAL_SIGNAL_EMISSION], g_quark_from_string (signal_name), signal_name, parameters);
}

/*
 * dfsm_machine_new:
 * @environment: a #DfsmEnvironment containing all the variables and functions used by the machine
 * @state_names: an array of strings of all the state names used in the DFSM
 * @transitions: an opaque array of structures representing each of the possible transitions in the DFSM
 *
 * Creates a new #DfsmMachine with the given environment, states and set of transitions.
 *
 * Return value: (transfer full): a new #DfsmMachine
 */
DfsmMachine *
_dfsm_machine_new (DfsmEnvironment *environment, GPtrArray/*<string>*/ *state_names, GPtrArray/*<DfsmAstTransition>*/ *transitions)
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
	machine->priv->transitions.arbitrarily_triggered = g_ptr_array_new_with_free_func (dfsm_ast_node_unref);

	for (i = 0; i < transitions->len; i++) {
		DfsmAstTransition *transition;

		transition = g_ptr_array_index (transitions, i);

		switch (transition->trigger) {
			case DFSM_AST_TRANSITION_METHOD_CALL: {
				GPtrArray/*<DfsmAstTransition>*/ *new_transitions;

				/* Add the transition to a new or existing array of transitions for the given method name. */
				new_transitions = g_hash_table_lookup (machine->priv->transitions.method_call_triggered,
				                                       transition->trigger_params.method_name);

				if (new_transitions == NULL) {
					new_transitions = g_ptr_array_new_with_free_func (dfsm_ast_node_unref);
					g_hash_table_insert (machine->priv->transitions.method_call_triggered,
					                     g_strdup (transition->trigger_params.method_name), new_transitions);
				}

				g_ptr_array_add (new_transitions, dfsm_ast_node_ref (transition));

				break;
			}
			case DFSM_AST_TRANSITION_ARBITRARY:
				/* Arbitrary transition */
				g_ptr_array_add (machine->priv->transitions.arbitrarily_triggered, dfsm_ast_node_ref (transition));
				break;
			default:
				g_assert_not_reached ();
		}
	}

	return machine;
}

/**
 * dfsm_machine_start_simulation:
 * @self: a #DfsmMachine
 *
 * Start the simulation. This permits dfsm_machine_call_method() to be called in order to trigger transitions by incoming D-Bus method calls. It also
 * starts a timer for arbitrary transitions to be triggered off.
 *
 * If the simulation is already running, this function returns immediately. Otherwise, #DfsmMachine:simulation-status will be set to
 * %DFSM_SIMULATION_STATUS_STARTED before this function returns.
 */
void
dfsm_machine_start_simulation (DfsmMachine *self)
{
	DfsmMachinePrivate *priv;

	g_return_if_fail (DFSM_IS_MACHINE (self));

	priv = self->priv;

	/* No-op? */
	if (priv->simulation_status == DFSM_SIMULATION_STATUS_STARTED) {
		return;
	}

	g_debug ("Starting the simulation.");

	/* Add a random timeout to the next potential arbitrary transition. */
	schedule_arbitrary_transition (self);

	/* Change simulation status. */
	priv->simulation_status = DFSM_SIMULATION_STATUS_STARTED;
	g_object_notify (G_OBJECT (self), "simulation-status");
}

/**
 * dfsm_machine_stop_simulation:
 * @self: a #DfsmMachine
 *
 * Stop the simulation. This stops the timer for triggering arbitrary permissions, and prohibits dfsm_machine_call_method() from being called again.
 *
 * If the simulation is already stopped, this function returns immediately. Otherwise, #DfsmMachine:simulation-status will be set to
 * %DFSM_SIMULATION_STATUS_STOPPED before this function returns.
 */
void
dfsm_machine_stop_simulation (DfsmMachine *self)
{
	DfsmMachinePrivate *priv;

	g_return_if_fail (DFSM_IS_MACHINE (self));

	priv = self->priv;

	/* No-op? */
	if (priv->simulation_status == DFSM_SIMULATION_STATUS_STOPPED) {
		return;
	}

	g_debug ("Stopping the simulation.");

	/* Cancel any outstanding potential arbitrary transition. */
	g_debug ("Cancelling outstanding arbitrary transitions.");
	g_source_remove (priv->timeout_id);
	priv->timeout_id = 0;

	/* Change simulation status. */
	priv->simulation_status = DFSM_SIMULATION_STATUS_STOPPED;
	g_object_notify (G_OBJECT (self), "simulation-status");
}

/**
 * dfsm_machine_call_method:
 * @self: a #DfsmMachine
 * @interface_name: the name of the D-Bus interface that @method_name is defined on
 * @method_name: the name of the D-Bus method to call
 * @parameters: parameters for the D-Bus method
 * @error: a #GError
 *
 * Call the given @method_name on the DFSM machine with the given @parameters as input. The method will be called synchronously, and its result returned
 * as a #GVariant. If the method throws an error, the return value will be %NULL and the error will be set in @error.
 *
 * Any signal emissions and property changes which occur as a result of the method call will be made while this function is running.
 *
 * Return value: (transfer full): non-floating return value from the method call, or %NULL
 */
GVariant *
dfsm_machine_call_method (DfsmMachine *self, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GError **error)
{
	DfsmMachinePrivate *priv;
	GPtrArray/*<DfsmAstTransition>*/ *possible_transitions;
	gboolean executed_transition = FALSE;
	GVariant *return_value = NULL;
	GDBusNodeInfo *node_info;
	GDBusInterfaceInfo *interface_info;
	GDBusMethodInfo *method_info;
	guint i;
	GError *child_error = NULL;

	g_return_val_if_fail (DFSM_IS_MACHINE (self), NULL);
	g_return_val_if_fail (interface_name != NULL && *interface_name != '\0', NULL);
	g_return_val_if_fail (method_name != NULL && *method_name != '\0', NULL);
	g_return_val_if_fail (parameters != NULL, NULL);
	g_return_val_if_fail (error != NULL && *error == NULL, NULL);

	priv = self->priv;

	/* Can't call a method if the simulation isn't running. */
	if (priv->simulation_status != DFSM_SIMULATION_STATUS_STARTED) {
		g_set_error (error, DFSM_SIMULATION_ERROR, DFSM_SIMULATION_ERROR_INVALID_STATUS,
		             "Can't call a D-Bus method if the simulation isn't running.");
		goto done;
	}

	/* Look up the method name in our set of transitions which are triggered by method calls */
	possible_transitions = g_hash_table_lookup (priv->transitions.method_call_triggered, method_name);

	if (possible_transitions == NULL || possible_transitions->len == 0) {
		/* Unknown method call. Spit out a warning and then return the unit tuple. If this is of the wrong type, then tough. We don't want
		 * to start trying to make up arbitrary data structures to match a given method return type. */
		g_warning ("Unrecognized method call to ‘%s’ on DFSM. Ignoring method call.", method_name);
		goto done;
	}

	/* Add the method's in parameters to the environment. */
	node_info = dfsm_environment_get_dbus_node_info (priv->environment);

	interface_info = g_dbus_node_info_lookup_interface (node_info, interface_name);

	if (interface_info == NULL) {
		g_warning ("Runtime error in simulation: Couldn't find interface containing method ‘%s’.", method_name);
		goto done;
	}

	method_info = g_dbus_interface_info_lookup_method (interface_info, method_name);

	if (method_info == NULL) {
		g_warning ("Runtime error in simulation: Couldn't find interface containing method ‘%s’.", method_name);
		goto done;
	}

	/* Add the parameters to the environment. */
	if (method_info->in_args != NULL) {
		for (i = 0; method_info->in_args[i] != NULL && i < g_variant_n_children (parameters); i++) {
			GVariant *parameter;

			/* Add the (i)th tuple child of the input parameters to the environment with the name given by the (i)th in argument in the
			 * method info. */
			parameter = g_variant_get_child_value (parameters, i);
			dfsm_environment_set_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, method_info->in_args[i]->name, parameter);
			g_variant_unref (parameter);
		}
	}

	if (method_info->in_args[i] != NULL || i < g_variant_n_children (parameters)) {
		g_warning ("Runtime error in simulation: mismatch between interface and input of in-args for method ‘%s’. Continuing.", method_name);
	}

	/* Find and potentially execute a transition from the array. */
	return_value = find_and_execute_random_transition (self, possible_transitions, &executed_transition, &child_error);

	/* Restore the environment. */
	if (method_info->in_args != NULL) {
		for (i = 0; method_info->in_args[i] != NULL && i < g_variant_n_children (parameters); i++) {
			/* Remove the variable with the name given by the (i)th in argument in the method info from the environment. */
			dfsm_environment_unset_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_LOCAL, method_info->in_args[i]->name);
		}
	}

done:
	/* If we failed to execute a transition, warn and return the unit tuple. */
	if (executed_transition == FALSE && child_error == NULL) {
		g_warning ("Failed to execute any DFSM transitions as a result of method call ‘%s’. Ignoring method call.", method_name);
		return_value = g_variant_new_tuple (NULL, 0);
	} else if (executed_transition == FALSE || return_value == NULL) {
		/* Error or precondition failure */
		g_propagate_error (error, child_error);
	}

	g_assert (return_value == NULL || g_variant_is_floating (return_value) == FALSE);
	g_assert ((return_value != NULL) != (child_error != NULL));

	return return_value;
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
