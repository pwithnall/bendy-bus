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

#ifndef DFSM_AST_OBJECT_H
#define DFSM_AST_OBJECT_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-node.h>
#include <dfsm/dfsm-ast-transition.h>

G_BEGIN_DECLS

/**
 * DfsmAstObjectStateNumber:
 *
 * A unique identifier for a DFSM state in a given #DfsmAstObject. Same as #DfsmMachineStateNumber.
 */
typedef guint DfsmAstObjectStateNumber; /* same as DfsmMachineStateNumber */

/**
 * DfsmAstObjectTransition:
 * @from_state: state number of the state the transition moves out of
 * @to_state: state number of the state the transition moves in to
 * @transition: preconditions and statements which form the transition
 * @nickname: (allow-none): user-provided nickname for this combination of @from_state, @to_state and @transition, or %NULL
 *
 * A wrapper for #DfsmAstTransition which specifies which states it moves between, and an optional user-provided nickname for this combination of
 * state numbers and transition. This allows the re-use of #DfsmAstTransition<!-- -->s (for example) when they're specified to be used inside several
 * states by the user.
 */
typedef struct {
	/*< public >*/
	DfsmAstObjectStateNumber from_state;
	DfsmAstObjectStateNumber to_state;
	DfsmAstTransition *transition;
	gchar *nickname;

	/*< private >*/
	gint ref_count;
} DfsmAstObjectTransition;

DfsmAstObjectTransition *dfsm_ast_object_transition_new (DfsmAstObjectStateNumber from_state, DfsmAstObjectStateNumber to_state,
                                                         DfsmAstTransition *transition, const gchar *nickname) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
DfsmAstObjectTransition *dfsm_ast_object_transition_ref (DfsmAstObjectTransition *object_transition);
void dfsm_ast_object_transition_unref (DfsmAstObjectTransition *object_transition);
gchar *dfsm_ast_object_transition_build_friendly_name (DfsmAstObjectTransition *object_transition) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

#define DFSM_TYPE_AST_OBJECT		(dfsm_ast_object_get_type ())
#define DFSM_AST_OBJECT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_OBJECT, DfsmAstObject))
#define DFSM_AST_OBJECT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_OBJECT, DfsmAstObjectClass))
#define DFSM_IS_AST_OBJECT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_OBJECT))
#define DFSM_IS_AST_OBJECT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_OBJECT))
#define DFSM_AST_OBJECT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_OBJECT, DfsmAstObjectClass))

typedef struct _DfsmAstObjectPrivate	DfsmAstObjectPrivate;

/**
 * DfsmAstObject:
 *
 * All the fields in the #DfsmAstObject structure are private and should never be accessed directly.
 */
typedef struct {
	DfsmAstNode parent;
	DfsmAstObjectPrivate *priv;
} DfsmAstObject;

/**
 * DfsmAstObjectClass:
 *
 * All the fields in the #DfsmAstObjectClass structure are private and should never be accessed directly.
 */
typedef struct {
	/*< private >*/
	DfsmAstNodeClass parent;
} DfsmAstObjectClass;

GType dfsm_ast_object_get_type (void) G_GNUC_CONST;

void dfsm_ast_object_initial_check (DfsmAstObject *self, GError **error);

DfsmEnvironment *dfsm_ast_object_get_environment (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_ast_object_get_state_names (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<DfsmAstObjectTransition>*/ *dfsm_ast_object_get_transitions (DfsmAstObject *self) G_GNUC_PURE;
const gchar *dfsm_ast_object_get_object_path (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_ast_object_get_well_known_bus_names (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_ast_object_get_interface_names (DfsmAstObject *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_AST_OBJECT_H */
