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

#ifndef DFSM_AST_TRANSITION_H
#define DFSM_AST_TRANSITION_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-node.h>
#include <dfsm/dfsm-output-sequence.h>

G_BEGIN_DECLS

/**
 * DfsmAstTransitionTrigger:
 * @DFSM_AST_TRANSITION_METHOD_CALL: triggered by an incoming D-Bus method call
 * @DFSM_AST_TRANSITION_PROPERTY_SET: triggered by a D-Bus property changing value
 * @DFSM_AST_TRANSITION_ARBITRARY: triggered at random intervals on a timer
 *
 * Types of triggers for a transition to occur.
 */
typedef enum {
	DFSM_AST_TRANSITION_METHOD_CALL,
	DFSM_AST_TRANSITION_PROPERTY_SET,
	DFSM_AST_TRANSITION_ARBITRARY,
} DfsmAstTransitionTrigger;

#define DFSM_TYPE_AST_TRANSITION		(dfsm_ast_transition_get_type ())
#define DFSM_AST_TRANSITION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_TRANSITION, DfsmAstTransition))
#define DFSM_AST_TRANSITION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_TRANSITION, DfsmAstTransitionClass))
#define DFSM_IS_AST_TRANSITION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_TRANSITION))
#define DFSM_IS_AST_TRANSITION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_TRANSITION))
#define DFSM_AST_TRANSITION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_TRANSITION, DfsmAstTransitionClass))

typedef struct _DfsmAstTransitionPrivate	DfsmAstTransitionPrivate;

/**
 * DfsmAstTransition:
 *
 * All the fields in the #DfsmAstTransition structure are private and should never be accessed directly.
 */
typedef struct {
	DfsmAstNode parent;
	DfsmAstTransitionPrivate *priv;
} DfsmAstTransition;

/**
 * DfsmAstTransitionClass:
 *
 * All the fields in the #DfsmAstTransitionClass structure are private and should never be accessed directly.
 */
typedef struct {
	/*< private >*/
	DfsmAstNodeClass parent;
} DfsmAstTransitionClass;

GType dfsm_ast_transition_get_type (void) G_GNUC_CONST;

GPtrArray/*<DfsmAstPrecondition>*/ *dfsm_ast_transition_get_preconditions (DfsmAstTransition *self) G_GNUC_PURE;
gboolean dfsm_ast_transition_check_preconditions (DfsmAstTransition *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence,
                                                  gboolean *will_throw_error);
void dfsm_ast_transition_execute (DfsmAstTransition *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence);

DfsmAstTransitionTrigger dfsm_ast_transition_get_trigger (DfsmAstTransition *self) G_GNUC_PURE;
const gchar *dfsm_ast_transition_get_trigger_method_name (DfsmAstTransition *self) G_GNUC_PURE;
const gchar *dfsm_ast_transition_get_trigger_property_name (DfsmAstTransition *self) G_GNUC_PURE;
gboolean dfsm_ast_transition_contains_throw_statement (DfsmAstTransition *self) G_GNUC_PURE;

GPtrArray *dfsm_ast_transition_get_statements (DfsmAstTransition *self) G_GNUC_PURE; /* array of DfsmAstStatements */

G_END_DECLS

#endif /* !DFSM_AST_TRANSITION_H */
