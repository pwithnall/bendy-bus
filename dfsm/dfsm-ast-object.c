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
 * SECTION:dfsm-ast-object
 * @short_description: AST object node
 * @stability: Unstable
 * @include: dfsm/dfsm-ast-object.h
 *
 * Top-level AST node which represents a D-Bus object in a simulation description. No simulation functionality is implemented in #DfsmAstObject, but it
 * does implement checking and validation of the AST.
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "dfsm-ast-data-structure.h"
#include "dfsm-ast-object.h"
#include "dfsm-ast-transition.h"
#include "dfsm-parser.h"
#include "dfsm-parser-internal.h"

/**
 * dfsm_ast_object_transition_new:
 * @from_state: state to transition out of
 * @to_state: state to transition into
 * @transition: the #DfsmAstTransition providing the code
 * @nickname: (allow-none): user-provided nickname for this combination of transition and states, or %NULL
 *
 * Create a new #DfsmAstObjectTransition wrapping the given parameters. The object is constructed with a reference count of 1.
 *
 * Return value: a new #DfsmAstObjectTransition wrapping the given parameters
 */
DfsmAstObjectTransition *
dfsm_ast_object_transition_new (DfsmMachineStateNumber from_state, DfsmMachineStateNumber to_state, DfsmAstTransition *transition, const gchar *nickname)
{
	DfsmAstObjectTransition *object_transition;

	g_return_val_if_fail (from_state != DFSM_MACHINE_INVALID_STATE, NULL);
	g_return_val_if_fail (to_state != DFSM_MACHINE_INVALID_STATE, NULL);
	g_return_val_if_fail (DFSM_IS_AST_TRANSITION (transition), NULL);

	object_transition = g_slice_new (DfsmAstObjectTransition);

	object_transition->from_state = from_state;
	object_transition->to_state = to_state;
	object_transition->transition = g_object_ref (transition);
	object_transition->nickname = g_strdup (nickname);
	object_transition->ref_count = 1;

	return object_transition;
}

/**
 * dfsm_ast_object_transition_ref:
 * @object_transition: a #DfsmAstObjectTransition
 *
 * Increase the reference count on the given @object_transition.
 *
 * Return value: the same pointer @object_transition, for convenience of chaining function calls
 */
DfsmAstObjectTransition *
dfsm_ast_object_transition_ref (DfsmAstObjectTransition *object_transition)
{
	g_assert (object_transition != NULL);

	g_atomic_int_inc (&object_transition->ref_count);

	return object_transition;
}

/**
 * dfsm_ast_object_transition_unref:
 * @object_transition: a #DfsmAstObjectTransition
 *
 * Decrease the reference count on the given @object_transition. If its reference count reaches 0, the @object_transition will be freed.
 */
void
dfsm_ast_object_transition_unref (DfsmAstObjectTransition *object_transition)
{
	g_assert (object_transition != NULL);

	if (g_atomic_int_dec_and_test (&object_transition->ref_count) == TRUE) {
		g_free (object_transition->nickname);
		g_object_unref (object_transition->transition);

		g_slice_free (DfsmAstObjectTransition, object_transition);
	}
}

/**
 * dfsm_ast_object_transition_build_friendly_name:
 * @object_transition: a #DfsmAstObjectTransition
 *
 * Build a friendly name for the @object_transition. This uses the transition's nickname, if set, and if not uses the value of the @object_transition
 * pointer. A non-empty string will always be returned by this function.
 *
 * Return value: (transfer full): a friendly name for the @object_transition; free with g_free()
 */
gchar *
dfsm_ast_object_transition_build_friendly_name (DfsmAstObjectTransition *object_transition)
{
	g_return_val_if_fail (object_transition != NULL, NULL);

	if (object_transition->nickname != NULL) {
		return g_strdup_printf (_("‘%s’ (%p)"), object_transition->nickname, object_transition->transition);
	}

	return g_strdup_printf ("%p", object_transition->transition);
}

static void dfsm_ast_object_dispose (GObject *object);
static void dfsm_ast_object_finalize (GObject *object);
static void dfsm_ast_object_sanity_check (DfsmAstNode *node);
static void dfsm_ast_object_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);
static void dfsm_ast_object_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstObjectPrivate {
	gchar *object_path;
	GPtrArray *bus_names; /* array of strings */
	GPtrArray *interface_names; /* array of strings */
	GDBusNodeInfo *dbus_node_info;
	DfsmEnvironment *environment; /* not built or populated until pre_check_and_register() */
	GPtrArray *states; /* array of strings (indexed by DfsmAstStateNumber), not populated until pre_check_and_register() */
	GPtrArray *transitions; /* array of DfsmAstObjectTransitions (no index), not populated until pre_check_and_register() */

	/* Temporary (only used until pre_check_and_register()) */
	GPtrArray/*<GHashTable<string,DfsmAstDataStructure>>*/ *data_blocks;
	GPtrArray/*<GPtrArray<string>>*/ *state_blocks;
	GPtrArray/*<DfsmParserTransitionBlock>*/ *transition_blocks;
};

G_DEFINE_TYPE (DfsmAstObject, dfsm_ast_object, DFSM_TYPE_AST_NODE)

static void
dfsm_ast_object_class_init (DfsmAstObjectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DfsmAstNodeClass *node_class = DFSM_AST_NODE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmAstObjectPrivate));

	gobject_class->dispose = dfsm_ast_object_dispose;
	gobject_class->finalize = dfsm_ast_object_finalize;

	node_class->sanity_check = dfsm_ast_object_sanity_check;
	node_class->pre_check_and_register = dfsm_ast_object_pre_check_and_register;
	node_class->check = dfsm_ast_object_check;
}

static void
dfsm_ast_object_init (DfsmAstObject *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_AST_OBJECT, DfsmAstObjectPrivate);
}

static void
dfsm_ast_object_dispose (GObject *object)
{
	DfsmAstObjectPrivate *priv = DFSM_AST_OBJECT (object)->priv;

	if (priv->bus_names != NULL) {
		g_ptr_array_unref (priv->bus_names);
		priv->bus_names = NULL;
	}

	if (priv->data_blocks != NULL) {
		g_ptr_array_unref (priv->data_blocks);
		priv->data_blocks = NULL;
	}

	if (priv->state_blocks != NULL) {
		g_ptr_array_unref (priv->state_blocks);
		priv->state_blocks = NULL;
	}

	if (priv->transition_blocks != NULL) {
		g_ptr_array_unref (priv->transition_blocks);
		priv->transition_blocks = NULL;
	}

	if (priv->transitions != NULL) {
		g_ptr_array_unref (priv->transitions);
		priv->transitions = NULL;
	}

	if (priv->states != NULL) {
		g_ptr_array_unref (priv->states);
		priv->states = NULL;
	}

	if (priv->dbus_node_info != NULL) {
		g_dbus_node_info_unref (priv->dbus_node_info);
		priv->dbus_node_info = NULL;
	}

	g_clear_object (&priv->environment);

	if (priv->interface_names != NULL) {
		g_ptr_array_unref (priv->interface_names);
		priv->interface_names = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_object_parent_class)->dispose (object);
}

static void
dfsm_ast_object_finalize (GObject *object)
{
	DfsmAstObjectPrivate *priv = DFSM_AST_OBJECT (object)->priv;

	g_free (priv->object_path);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_ast_object_parent_class)->finalize (object);
}

static void
dfsm_ast_object_sanity_check (DfsmAstNode *node)
{
	DfsmAstObjectPrivate *priv = DFSM_AST_OBJECT (node)->priv;
	guint i;

	g_assert (priv->object_path != NULL);

	g_assert (priv->bus_names != NULL);

	for (i = 0; i < priv->bus_names->len; i++) {
		const gchar *bus_name;

		bus_name = g_ptr_array_index (priv->bus_names, i);
		g_assert (bus_name != NULL && *bus_name != '\0');
	}

	g_assert (priv->interface_names != NULL);

	for (i = 0; i < priv->interface_names->len; i++) {
		const gchar *interface_name;

		interface_name = g_ptr_array_index (priv->interface_names, i);
		g_assert (interface_name != NULL);
	}

	g_assert (priv->dbus_node_info != NULL);
	g_assert (priv->environment == NULL || DFSM_IS_ENVIRONMENT (priv->environment));
	/* TODO: Check all variable names and values in ->environment are non-NULL */

	g_assert (priv->states != NULL);

	for (i = 0; i < priv->states->len; i++) {
		const gchar *state_name;

		state_name = g_ptr_array_index (priv->states, i);
		g_assert (state_name != NULL);
	}

	g_assert (priv->transitions != NULL);

	for (i = 0; i < priv->transitions->len; i++) {
		DfsmAstObjectTransition *object_transition;

		object_transition = g_ptr_array_index (priv->transitions, i);

		g_assert (object_transition != NULL);
		g_assert (DFSM_IS_AST_TRANSITION (object_transition->transition));
	}

	/* TODO: data_blocks and state_blocks */

	if (priv->transition_blocks != NULL) {
		for (i = 0; i < priv->transition_blocks->len; i++) {
			guint j;
			DfsmParserTransitionBlock *transition_block = g_ptr_array_index (priv->transition_blocks, i);

			g_assert (transition_block != NULL);
			g_assert (DFSM_IS_AST_TRANSITION (transition_block->transition));
			dfsm_ast_node_sanity_check (DFSM_AST_NODE (transition_block->transition));

			g_assert (transition_block->state_pairs != NULL);
			g_assert (transition_block->state_pairs->len > 0);

			for (j = 0; j < transition_block->state_pairs->len; j++) {
				DfsmParserStatePair *state_pair = g_ptr_array_index (transition_block->state_pairs, j);
				g_assert (state_pair->from_state_name != NULL);
				g_assert (state_pair->to_state_name != NULL);
			}
		}
	}
}

static void
dfsm_ast_object_pre_check_and_register (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstObjectPrivate *priv = DFSM_AST_OBJECT (node)->priv;
	guint i;
	GPtrArray *states;
	GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces = NULL;
	GHashTable/*<string, DfsmMachineStateNumber>*/ *state_numbers = NULL;

	g_assert (environment == NULL); /* we haven't created this yet */

	if (g_variant_is_object_path (priv->object_path) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus object path: %s"), priv->object_path);
		goto done;
	}

	for (i = 0; i < priv->bus_names->len; i++) {
		const gchar *bus_name;

		bus_name = g_ptr_array_index (priv->bus_names, i);

		if (g_dbus_is_name (bus_name) == FALSE || g_dbus_is_unique_name (bus_name) == TRUE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus well-known bus name: %s"), bus_name);
			goto done;
		}
	}

	/* Must have at least one interface implemented. */
	if (priv->interface_names->len == 0) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
		             _("At least one interface must be implemented on every D-Bus object."));
		goto done;
	}

	/* Check and build an array of relevant GDBusInterfaceInfo structures. */
	interfaces = g_ptr_array_new_with_free_func ((GDestroyNotify) g_dbus_interface_info_unref);

	for (i = 0; i < priv->interface_names->len; i++) {
		guint f;
		const gchar *interface_name;
		GDBusInterfaceInfo *interface_info;

		interface_name = g_ptr_array_index (priv->interface_names, i);

		/* Valid interface name? */
		if (g_dbus_is_interface_name (interface_name) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid D-Bus interface name: %s"), interface_name);
			goto done;
		}

		/* Duplicates? */
		for (f = i + 1; f < priv->interface_names->len; f++) {
			if (strcmp (interface_name, g_ptr_array_index (priv->interface_names, f)) == 0) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Duplicate D-Bus interface name: %s"),
				             interface_name);
				goto done;
			}
		}

		/* Defined in the node info? */
		interface_info = g_dbus_node_info_lookup_interface (priv->dbus_node_info, interface_name);

		if (interface_info == NULL) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Unknown D-Bus interface name: %s"), interface_name);
			goto done;
		}

		/* Add it to the interface list. */
		g_ptr_array_add (interfaces, g_dbus_interface_info_ref (interface_info));
	}

	/* Build the environment. */
	priv->environment = _dfsm_environment_new (interfaces);

	/* Check all variable names in ->environment are valid names, and recursively check their values are acceptable. */
	for (i = 0; i < priv->data_blocks->len; i++) {
		GHashTable *data_block;
		GHashTableIter iter;
		const gchar *key;
		DfsmAstDataStructure *value_data_structure;

		data_block = g_ptr_array_index (priv->data_blocks, i);
		g_hash_table_iter_init (&iter, data_block);

		while (g_hash_table_iter_next (&iter, (gpointer*) &key, (gpointer*) &value_data_structure) == TRUE) {
			/* Valid variable name? */
			if (dfsm_is_variable_name (key) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid variable name: %s"), key);
				goto done;
			}

			/* Check for duplicates */
			if (dfsm_environment_has_variable (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, key) == TRUE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Duplicate variable name: %s"), key);
				goto done;
			}

			/* Check the value expression. */
			dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (value_data_structure), priv->environment, error);

			if (*error != NULL) {
				goto done;
			}
		}
	}

	/* States. Add the last state of the first state block to the priv->states array first, since it's the first state listed in the source
	 * file (it just appears in a different position due to the way the parser's recursion is implemented). i.e. It's the main state. */
	if (priv->state_blocks->len == 0 || ((GPtrArray*) g_ptr_array_index (priv->state_blocks, 0))->len == 0) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("A default state is required."));
		goto done;
	}

	/* While we're at it, create a temporary hash table which maps state names (strings) to their allocated state numbers. This is useful for
	 * duplicate detection, and for quickly looking up the state numbers corresponding to the state names when we process transitions below.
	 * Note that its keys are only valid while priv->state_blocks is still alive. */
	state_numbers = g_hash_table_new (g_str_hash, g_str_equal);

	states = g_ptr_array_index (priv->state_blocks, 0);
	g_ptr_array_add (priv->states, g_strdup ((gchar*) g_ptr_array_index (states, states->len - 1)));
	g_hash_table_insert (state_numbers, g_ptr_array_index (states, states->len - 1), GUINT_TO_POINTER (0));

	for (i = 0; i < priv->state_blocks->len; i++) {
		guint f;

		states = g_ptr_array_index (priv->state_blocks, i);

		for (f = 0; f < states->len; f++) {
			const gchar *state_name;

			/* Skip the main state. */
			if (i == 0 && f == states->len - 1) {
				continue;
			}

			state_name = g_ptr_array_index (states, f);

			/* Valid state name? */
			if (dfsm_is_state_name (state_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid state name: %s"), state_name);
				goto done;
			}

			/* Check for duplicates */
			if (g_hash_table_lookup_extended (state_numbers, state_name, NULL, NULL) == TRUE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Duplicate state name: %s"), state_name);
				goto done;
			}

			g_ptr_array_add (priv->states, g_strdup (state_name));
			g_hash_table_insert (state_numbers, (gpointer) state_name, GUINT_TO_POINTER (priv->states->len - 1));
		}
	}

	for (i = 0; i < priv->transition_blocks->len; i++) {
		DfsmParserTransitionBlock *transition_block;
		guint j;

		transition_block = g_ptr_array_index (priv->transition_blocks, i);

		/* Check the transition itself first. */
		dfsm_ast_node_pre_check_and_register (DFSM_AST_NODE (transition_block->transition), priv->environment, error);

		if (*error != NULL) {
			goto done;
		}

		/* Register it against all its listed state pairs. */
		for (j = 0; j < transition_block->state_pairs->len; j++) {
			const DfsmParserStatePair *state_pair;
			gpointer state_number_ptr;
			DfsmMachineStateNumber from_state_number, to_state_number;
			DfsmAstObjectTransition *object_transition;

			state_pair = g_ptr_array_index (transition_block->state_pairs, j);

			/* Validate the state names. */
			if (dfsm_is_state_name (state_pair->from_state_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid ‘from’ state name: %s"),
				             state_pair->from_state_name);
				goto done;
			} else if (dfsm_is_state_name (state_pair->to_state_name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Invalid ‘to’ state name: %s"),
				             state_pair->to_state_name);
				goto done;
			}

			/* Look up the allocated numbers of these state names. */
			if (g_hash_table_lookup_extended (state_numbers, state_pair->from_state_name, NULL, &state_number_ptr) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Unknown ‘from’ state name: %s"),
				             state_pair->from_state_name);
				goto done;
			} else {
				from_state_number = GPOINTER_TO_UINT (state_number_ptr);
			}

			if (g_hash_table_lookup_extended (state_numbers, state_pair->to_state_name, NULL, &state_number_ptr) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, _("Unknown ‘to’ state name: %s"),
				             state_pair->to_state_name);
				goto done;
			} else {
				to_state_number = GPOINTER_TO_UINT (state_number_ptr);
			}

			/* Register the transition. */
			object_transition = dfsm_ast_object_transition_new (from_state_number, to_state_number, transition_block->transition,
			                                                    state_pair->nickname);
			g_ptr_array_add (priv->transitions, object_transition);
		}
	}

done:
	g_ptr_array_unref (priv->state_blocks);
	priv->state_blocks = NULL;

	if (state_numbers != NULL) {
		g_hash_table_unref (state_numbers);
	}

	if (interfaces != NULL) {
		g_ptr_array_unref (interfaces);
		interfaces = NULL;
	}
}

/**
 * dfsm_ast_object_initial_check:
 * @self: a #DfsmAstObject
 * @error: a #GError, or %NULL
 *
 * Run the initial checks on the #DfsmAstObject, and generate its environment. For #DfsmAstObject<!-- -->s, this must be run instead of
 * dfsm_ast_node_sanity_check() and dfsm_ast_node_pre_check_and_register().
 */
void
dfsm_ast_object_initial_check (DfsmAstObject *self, GError **error)
{
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_AST_OBJECT (self));
	g_return_if_fail (error == NULL || *error == NULL);

	/* Sanity check, then run the pre-check without an environment. */
	dfsm_ast_object_sanity_check (DFSM_AST_NODE (self));
	dfsm_ast_object_pre_check_and_register (DFSM_AST_NODE (self), NULL, &child_error);

	/* We pass in our own child_error so that we can guarantee it's non-NULL to the virtual method implementations. */
	if (child_error != NULL) {
		g_propagate_error (error, child_error);
	}
}

static void
dfsm_ast_object_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstObjectPrivate *priv = DFSM_AST_OBJECT (node)->priv;
	guint i;
	GPtrArray/*<GDBusInterfaceInfo>*/ *interfaces;

	/* Check the value expressions for each variable, then evaluate them and set the final values in the environment. Only then can we free
	 * ->data_blocks. */
	for (i = 0; i < priv->data_blocks->len; i++) {
		GHashTable *data_block;
		GHashTableIter iter;
		const gchar *key;
		DfsmAstDataStructure *value_data_structure;

		data_block = g_ptr_array_index (priv->data_blocks, i);
		g_hash_table_iter_init (&iter, data_block);

		while (g_hash_table_iter_next (&iter, (gpointer*) &key, (gpointer*) &value_data_structure) == TRUE) {
			GVariantType *new_type;
			GVariant *new_value;

			/* Check the value expression. */
			dfsm_ast_node_check (DFSM_AST_NODE (value_data_structure), priv->environment, error);

			if (*error != NULL) {
				return;
			}

			/* Calculate the data structure's type. */
			new_type = dfsm_ast_data_structure_calculate_type (value_data_structure, priv->environment);

			/* Evaluate the value expression. */
			new_value = dfsm_ast_data_structure_to_variant (value_data_structure, priv->environment);

			/* Store the real type and value in the environment. */
			dfsm_environment_set_variable_type (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, key, new_type);
			dfsm_environment_set_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, key, new_value);

			g_variant_unref (new_value);
			g_variant_type_free (new_type);
		}
	}

	g_ptr_array_unref (priv->data_blocks);
	priv->data_blocks = NULL;

	/* Check the object defines object-level variables for each of the properties of its interfaces. The set of interfaces in the environment
	 * should be equal to those in ->interface_names. */
	interfaces = dfsm_environment_get_interfaces (environment);

	for (i = 0; i < interfaces->len; i++) {
		GDBusInterfaceInfo *interface_info;
		GDBusPropertyInfo **property_infos;

		interface_info = (GDBusInterfaceInfo*) g_ptr_array_index (interfaces, i);

		if (interface_info->properties == NULL) {
			continue;
		}

		for (property_infos = interface_info->properties; *property_infos != NULL; property_infos++) {
			GVariantType *environment_type, *introspected_type;

			/* Variable exists for the property? */
			if (dfsm_environment_has_variable (environment, DFSM_VARIABLE_SCOPE_OBJECT, (*property_infos)->name) == FALSE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("D-Bus property without corresponding object variable: %s"), (*property_infos)->name);
				return;
			}

			/* Types match exactly (sub- or super-typing isn't allowed, since the variable has to be got and set)? */
			environment_type = dfsm_environment_dup_variable_type (environment, DFSM_VARIABLE_SCOPE_OBJECT, (*property_infos)->name);
			g_assert (g_variant_type_string_is_valid ((*property_infos)->signature));
			introspected_type = g_variant_type_new ((*property_infos)->signature);

			if (g_variant_type_equal (environment_type, introspected_type) == FALSE) {
				gchar *expected_type_string;

				expected_type_string = g_variant_type_dup_string (environment_type);
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             _("Incorrect type for object variable ‘%s’ corresponding to D-Bus property: expected ‘%s’ but got ‘%s’."),
				             (*property_infos)->name, (*property_infos)->signature, expected_type_string);
				g_free (expected_type_string);

				g_variant_type_free (introspected_type);
				g_variant_type_free (environment_type);

				return;
			}

			g_variant_type_free (introspected_type);
			g_variant_type_free (environment_type);
		}
	}

	/* Check each transition. */
	for (i = 0; i < priv->transitions->len; i++) {
		DfsmAstObjectTransition *object_transition;

		object_transition = g_ptr_array_index (priv->transitions, i);

		dfsm_ast_node_check (DFSM_AST_NODE (object_transition->transition), environment, error);

		if (*error != NULL) {
			return;
		}
	}
}

/**
 * dfsm_ast_object_new:
 * @dbus_node_info: introspection data about the D-Bus interfaces used by the object
 * @object_path: the object's D-Bus path
 * @bus_names: a (potentially empty) array of strings containing the well-known bus names the object should own
 * @interface_names: an array of strings containing the names of the D-Bus interfaces implemented by the object
 * @data_blocks: an array of #GHashTable<!-- -->s containing the object-level variables in the object
 * @state_blocks: an array of #GPtrArray<!-- -->s of strings containing the names of the possible states of the object
 * @transition_blocks: an array of #DfsmParserTransitionBlock<!-- -->s containing the object's possible state transitions
 *
 * Create a new #DfsmAstObject AST node.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstObject *
dfsm_ast_object_new (GDBusNodeInfo *dbus_node_info, const gchar *object_path, GPtrArray/*<string>*/ *bus_names, GPtrArray/*<string>*/ *interface_names,
                     GPtrArray/*<GHashTable<string,DfsmAstDataStructure>>*/ *data_blocks, GPtrArray/*<GPtrArray<string>>*/ *state_blocks,
                     GPtrArray/*<DfsmParserTransitionBlock>*/ *transition_blocks)
{
	DfsmAstObject *object;
	DfsmAstObjectPrivate *priv;

	g_return_val_if_fail (dbus_node_info != NULL, NULL);
	g_return_val_if_fail (object_path != NULL && *object_path != '\0', NULL);
	g_return_val_if_fail (bus_names != NULL, NULL);
	g_return_val_if_fail (interface_names != NULL, NULL);
	g_return_val_if_fail (data_blocks != NULL, NULL);
	g_return_val_if_fail (state_blocks != NULL, NULL);
	g_return_val_if_fail (transition_blocks != NULL, NULL);

	object = g_object_new (DFSM_TYPE_AST_OBJECT, NULL);
	priv = object->priv;

	priv->object_path = g_strdup (object_path);
	priv->bus_names = g_ptr_array_ref (bus_names);
	priv->interface_names = g_ptr_array_ref (interface_names);
	priv->dbus_node_info = g_dbus_node_info_ref (dbus_node_info);
	priv->environment = NULL; /* build in pre_check_and_register() */
	priv->states = g_ptr_array_new_with_free_func (g_free);
	priv->transitions = g_ptr_array_new_with_free_func ((GDestroyNotify) dfsm_ast_object_transition_unref);

	priv->data_blocks = g_ptr_array_ref (data_blocks);
	priv->state_blocks = g_ptr_array_ref (state_blocks);
	priv->transition_blocks = g_ptr_array_ref (transition_blocks);

	return object;

	return NULL;
}

/**
 * dfsm_ast_object_get_environment:
 * @self: a #DfsmAstObject
 *
 * Gets the environment associated with the #DfsmAstObject.
 *
 * Return value: the object's #DfsmEnvironment
 */
DfsmEnvironment *
dfsm_ast_object_get_environment (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->environment;
}

/**
 * dfsm_ast_object_get_state_names:
 * @self: a #DfsmAstObject
 *
 * Gets the array of human-readable state names for all the states in the object. Each state name is indexed by its state number
 * (#DfsmAstObjectStateNumber). Each state name is a UTF-8 string.
 *
 * Return value: (transfer none) (element-type utf8): array of state names
 */
GPtrArray/*<string>*/ *
dfsm_ast_object_get_state_names (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->states;
}

/**
 * dfsm_ast_object_get_transitions:
 * @self: a #DfsmAstObject
 *
 * Gets the array of all the transitions in the object. The array is in no particular order.
 *
 * Return value: (transfer none) (element-type utf8): array of #DfsmAstObjectTransitions
 */
GPtrArray/*<GPtrArray<DfsmAstObjectTransitionPair>>*/ *
dfsm_ast_object_get_transitions (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->transitions;
}

/**
 * dfsm_ast_object_get_object_path:
 * @self: a #DfsmAstObject
 *
 * Gets the D-Bus object path the object should be exposed on when added to the bus. This will always be set.
 *
 * Return value: D-Bus object path for the object
 */
const gchar *
dfsm_ast_object_get_object_path (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->object_path;
}

/**
 * dfsm_ast_object_get_well_known_bus_names:
 * @self: a #DfsmAstObject
 *
 * Gets an array of strings containing the D-Bus well-known bus names which should be owned by the object. This array may be empty, but will never be
 * %NULL.
 *
 * Return value: (transfer none) (element-type utf8): array of D-Bus well-known bus names
 */
GPtrArray/*<string>*/ *
dfsm_ast_object_get_well_known_bus_names (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->bus_names;
}

/**
 * dfsm_ast_object_get_interface_names:
 * @self: a #DfsmAstObject
 *
 * Gets an array of strings containing the D-Bus interface names which are implemented by the object. This array should always have at least one
 * entry, and will never be %NULL.
 *
 * Return value: (transfer none) (element-type utf8): array of D-Bus interface names
 */
GPtrArray/*<string>*/ *
dfsm_ast_object_get_interface_names (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->interface_names;
}
