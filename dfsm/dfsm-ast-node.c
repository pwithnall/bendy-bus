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
 * dfsm_ast_node_sanity_check:
 * @self: a #DfsmAstNode
 *
 * Sanity check the node (but not its descendents), asserting that various conditions hold which should always be true, regardless of the stage of
 * interpretation the system's in. If any of the assertions fail, the program will abort.
 *
 * The checks made by this function should always hold, regardless of user input.
 */
void
dfsm_ast_node_sanity_check (DfsmAstNode *self)
{
	DfsmAstNodeClass *klass;

	g_return_if_fail (DFSM_IS_AST_NODE (self));

	klass = DFSM_AST_NODE_GET_CLASS (self);

	/* If the sanity check function doesn't exist, assume the node is always guaranteed to be OK. */
	if (klass->sanity_check != NULL) {
		klass->sanity_check (self);
	}
}

/**
 * dfsm_ast_node_pre_check_and_register:
 * @self: a #DfsmAstNode
 * @environment: environment to register variables in
 * @error: a #GError
 *
 * Check the node and its descendents are basically correct. The checks guarantee enough is correct for appropriate additions to be made to the
 * @environment, which are performed by this function. If checking finds a problem, @error will be set to a suitable #GError; otherwise, @error will
 * remain unset. If a problem is found, @environment will be left in an unspecified state.
 *
 * The checks made by this function may not hold, depending on user input.
 */
void
dfsm_ast_node_pre_check_and_register (DfsmAstNode *self, DfsmEnvironment *environment, GError **error)
{
	DfsmAstNodeClass *klass;
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_AST_NODE (self));
	g_return_if_fail (DFSM_IS_ENVIRONMENT (environment));
	g_return_if_fail (error != NULL && *error == NULL);

	klass = DFSM_AST_NODE_GET_CLASS (self);

	/* If the pre-check function doesn't exist, assume the node is always guaranteed to be OK and doesn't need to register anything in the
	 * environment. */
	if (klass->pre_check_and_register != NULL) {
		klass->pre_check_and_register (self, environment, &child_error);

		/* We pass in our own child_error so that we can guarantee it's non-NULL to the virtual method implementations. */
		if (child_error != NULL) {
			g_propagate_error (error, child_error);
		}
	}
}

/**
 * dfsm_ast_node_check:
 * @self: a #DfsmAstNode
 * @environment: environment containing the values of all variables known so far
 * @error: a #GError
 *
 * Check the node and its descendents are correct. This may, for example, involve type checking or checking of constants. If checking finds a problem,
 * @error will be set to a suitable #GError; otherwise, @error will remain unset.
 *
 * This should only be called after dfsm_ast_node_pre_check_and_register() has been successfully called for all nodes, since this node's validity may
 * depend on other nodes having registered things in the @environment.
 *
 * The checks made by this function may not hold, depending on user input.
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
