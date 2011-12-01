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

#include "dfsm-ast-node.h"

G_DEFINE_ABSTRACT_TYPE (DfsmAstNode, dfsm_ast_node, G_TYPE_OBJECT)

static void
dfsm_ast_node_class_init (DfsmAstNodeClass *klass)
{
	/* Nothing to see here. */
}

static void
dfsm_ast_node_init (DfsmAstNode *self)
{
	/* Nothing to see here. */
}

/**
 * dfsm_ast_node_check:
 * @self: a #DfsmAstNode
 * @environment: environment containing the values of all variables known so far
 * @error: a #GError
 *
 * Check the node and its descendents are correct. This may, for example, involve type checking or checking of constants. If checking finds a problem,
 * @error will be set to a suitable #GError; otherwise, @error will remain unset.
 */
void
dfsm_ast_node_check (DfsmAstNode *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstNodeClass *klass;
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_AST_NODE (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (error != NULL && *error == NULL);

	klass = DFSM_AST_NODE_GET_CLASS (self);

	/* If the check function doesn't exist, assume the node is always guaranteed to be OK. */
	if (klass->check != NULL) {
		klass->check (self, environment, &child_error);

		/* We pass in our own child_error so that we can guarantee it's non-NULL to the virtual method implementations. */
		if (child_error != NULL) {
			g_propagate_error (error, child_error);
		}
	}
}
