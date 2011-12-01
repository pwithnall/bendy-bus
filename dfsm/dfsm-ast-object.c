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

#include "dfsm-ast-data-structure.h"
#include "dfsm-ast-object.h"
#include "dfsm-ast-transition.h"
#include "dfsm-parser.h"

static void dfsm_ast_object_dispose (GObject *object);
static void dfsm_ast_object_finalize (GObject *object);
static void dfsm_ast_object_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error);

struct _DfsmAstObjectPrivate {
	gchar *object_path;
	GPtrArray *interface_names; /* array of strings */
	DfsmEnvironment *environment;
	GPtrArray *states; /* array of strings (indexed by DfsmAstStateNumber) */
	GPtrArray *transitions; /* array of DfsmAstTransitions */
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

	if (priv->transitions != NULL) {
		g_ptr_array_unref (priv->transitions);
		priv->transitions = NULL;
	}

	if (priv->states != NULL) {
		g_ptr_array_unref (priv->states);
		priv->states = NULL;
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
dfsm_ast_object_check (DfsmAstNode *node, DfsmEnvironment *environment, GError **error)
{
	DfsmAstObjectPrivate *priv = DFSM_AST_OBJECT (node)->priv;
	guint i;

	/* Conditions which should always hold, regardless of user input. */
	g_assert (priv->object_path != NULL);
	g_assert (priv->interface_names != NULL);
	g_assert (DFSM_IS_ENVIRONMENT (priv->environment));
	g_assert (priv->states != NULL);

	for (i = 0; i < priv->interface_names->len; i++) {
		const gchar *interface_name;

		interface_name = g_ptr_array_index (priv->interface_names, i);
		g_assert (interface_name != NULL);
	}

	/* TODO: Check all variable names and values in ->environment are non-NULL */

	for (i = 0; i < priv->states->len; i++) {
		const gchar *state_name;

		state_name = g_ptr_array_index (priv->states, i);
		g_assert (state_name != NULL);
	}

	for (i = 0; i < priv->transitions->len; i++) {
		DfsmAstTransition *transition;

		transition = g_ptr_array_index (priv->transitions, i);
		g_assert (transition != NULL);
	}

	/* Conditions which may not hold as a result of invalid user input. */
	if (g_variant_is_object_path (priv->object_path) == FALSE) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus object path: %s", priv->object_path);
		return;
	}

	for (i = 0; i < priv->interface_names->len; i++) {
		guint f;
		const gchar *interface_name;

		interface_name = g_ptr_array_index (priv->interface_names, i);

		/* Valid interface name? */
		if (g_dbus_is_interface_name (interface_name) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid D-Bus interface name: %s", interface_name);
			return;
		}

		/* Duplicates? */
		for (f = i + 1; f < priv->interface_names->len; f++) {
			if (strcmp (interface_name, g_ptr_array_index (priv->interface_names, f)) == 0) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate D-Bus interface name: %s",
				             interface_name);
				return;
			}
		}
	}

	/* TODO: Check all variable names in ->environment are valid names, and recursively check their values are acceptable */
	/* TODO: Assert that object explicitly lists data items for each of the properties of its interfaces. */

	if (priv->states->len == 0) {
		g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "A default state is required.");
		return;
	}

	for (i = 0; i < priv->states->len; i++) {
		guint f;
		const gchar *state_name;

		state_name = g_ptr_array_index (priv->states, i);

		/* Valid state name? */
		if (dfsm_is_state_name (state_name) == FALSE) {
			g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Invalid state name: %s", state_name);
			return;
		}

		/* Duplicates? */
		for (f = i + 1; f < priv->states->len; f++) {
			if (strcmp (state_name, g_ptr_array_index (priv->states, f)) == 0) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate state name: %s", state_name);
				return;
			}
		}
	}

	for (i = 0; i < priv->transitions->len; i++) {
		DfsmAstTransition *transition;

		transition = g_ptr_array_index (priv->transitions, i);

		dfsm_ast_node_check ((DfsmAstNode*) transition, environment, error);

		if (*error != NULL) {
			return;
		}
	}
}

/**
 * dfsm_ast_object_new:
 * @dbus_node_info: introspection data about the D-Bus interfaces used by the object
 * @object_path: the object's D-Bus path
 * @interface_names: an array of strings containing the names of the D-Bus interfaces implemented by the object
 * @data_blocks: an array of #GHashTable<!-- -->s containing the object-level variables in the object
 * @state_blocks: an array of #GPtrArray<!-- -->s of strings containing the names of the possible states of the object
 * @transition_blocks: an array of #DfsmAstTransition<!-- -->s containing the object's possible state transitions
 * @error: (allow-none): a #GError, or %NULL
 *
 * Create a new #DfsmAstObject AST node.
 *
 * Return value: (transfer full): a new AST node
 */
DfsmAstObject *
dfsm_ast_object_new (GDBusNodeInfo *dbus_node_info, const gchar *object_path, GPtrArray/*<string>*/ *interface_names,
                     GPtrArray/*<GHashTable<string,DfsmAstDataStructure>>*/ *data_blocks, GPtrArray/*<GPtrArray>*/ *state_blocks,
                     GPtrArray/*<DfsmAstTransition>*/ *transition_blocks, GError **error)
{
	DfsmAstObject *object;
	DfsmAstObjectPrivate *priv;
	guint i;
	GPtrArray *states;

	g_return_val_if_fail (dbus_node_info != NULL, NULL);
	g_return_val_if_fail (object_path != NULL && *object_path != '\0', NULL);
	g_return_val_if_fail (interface_names != NULL, NULL);
	g_return_val_if_fail (data_blocks != NULL, NULL);
	g_return_val_if_fail (state_blocks != NULL, NULL);
	g_return_val_if_fail (transition_blocks != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	object = g_object_new (DFSM_TYPE_AST_OBJECT, NULL);
	priv = object->priv;

	priv->object_path = g_strdup (object_path);
	priv->interface_names = g_ptr_array_ref (interface_names);
	priv->environment = _dfsm_environment_new (dbus_node_info);
	priv->states = g_ptr_array_new_with_free_func (g_free);
	priv->transitions = g_ptr_array_ref (transition_blocks);

	/* Data items */
	for (i = 0; i < data_blocks->len; i++) {
		GHashTable *data_block;
		GHashTableIter iter;
		const gchar *key;
		DfsmAstDataStructure *value_data_structure;

		data_block = g_ptr_array_index (data_blocks, i);
		g_hash_table_iter_init (&iter, data_block);

		while (g_hash_table_iter_next (&iter, (gpointer*) &key, (gpointer*) &value_data_structure) == TRUE) {
			GVariant *new_value;
			GError *child_error = NULL;

			/* Check for duplicates */
			if (dfsm_environment_has_variable (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, key) == TRUE) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate variable name: %s", key);

				goto error;
			}

			/* Evaluate the value expression. */
			new_value = dfsm_ast_data_structure_to_variant (value_data_structure, priv->environment, &child_error);

			if (child_error != NULL) {
				g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID,
				             "Couldn't evaluate default value for variable ‘%s’: %s", key, child_error->message);
				g_error_free (child_error);

				goto error;
			}

			/* Store the value in the environment. */
			dfsm_environment_set_variable_value (priv->environment, DFSM_VARIABLE_SCOPE_OBJECT, key, new_value);

			g_variant_unref (new_value);
		}
	}

	/* States. Add the last state of the first state block to the priv->states array first, since it's the first state listed in the source
	 * file (it just appears in a different position due to the way the parser's recursion is implemented). i.e. It's the main state. */
	/* TODO: We assume we have at least one state block here, containing at least one state name. */
	states = g_ptr_array_index (state_blocks, 0);
	g_ptr_array_add (priv->states, g_strdup ((gchar*) g_ptr_array_index (states, states->len - 1)));

	for (i = 0; i < state_blocks->len; i++) {
		guint f;

		states = g_ptr_array_index (state_blocks, i);

		for (f = 0; f < states->len; f++) {
			const gchar *state_name;
			guint g;

			/* Skip the main state. */
			if (i == 0 && f == states->len - 1) {
				continue;
			}

			state_name = g_ptr_array_index (states, f);

			/* Check for duplicates */
			for (g = 0; g < priv->states->len; g++) {
				if (strcmp (state_name, g_ptr_array_index (priv->states, g)) == 0) {
					g_set_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_AST_INVALID, "Duplicate state name: %s", state_name);

					goto error;
				}
			}

			g_ptr_array_add (priv->states, g_strdup (state_name));
		}
	}

	return object;

error:
	/* Free everything and run away. */
	g_object_unref (object);

	return NULL;
}

/**
 * dfsm_ast_object_get_environment:
 * @self: a #DfsmAstObject
 *
 * TODO
 *
 * Return value: TODO
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
 * TODO
 *
 * Return value: TODO
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
 * TODO
 *
 * Return value: TODO
 */
GPtrArray/*<DfsmAstTransition>*/ *
dfsm_ast_object_get_transitions (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->transitions;
}

/**
 * dfsm_ast_object_get_object_path:
 * @self: a #DfsmAstObject
 *
 * TODO
 *
 * Return value: TODO
 */
const gchar *
dfsm_ast_object_get_object_path (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->object_path;
}

/**
 * dfsm_ast_object_get_interface_names:
 * @self: a #DfsmAstObject
 *
 * TODO
 *
 * Return value: TODO
 */
GPtrArray/*<string>*/ *
dfsm_ast_object_get_interface_names (DfsmAstObject *self)
{
	g_return_val_if_fail (DFSM_IS_AST_OBJECT (self), NULL);

	return self->priv->interface_names;
}