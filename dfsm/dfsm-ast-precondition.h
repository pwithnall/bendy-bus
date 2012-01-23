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

#ifndef DFSM_AST_PRECONDITION_H
#define DFSM_AST_PRECONDITION_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-node.h>
#include <dfsm/dfsm-ast-expression.h>
#include <dfsm/dfsm-environment.h>
#include <dfsm/dfsm-output-sequence.h>

G_BEGIN_DECLS

#define DFSM_TYPE_AST_PRECONDITION		(dfsm_ast_precondition_get_type ())
#define DFSM_AST_PRECONDITION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_PRECONDITION, DfsmAstPrecondition))
#define DFSM_AST_PRECONDITION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_PRECONDITION, DfsmAstPreconditionClass))
#define DFSM_IS_AST_PRECONDITION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_PRECONDITION))
#define DFSM_IS_AST_PRECONDITION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_PRECONDITION))
#define DFSM_AST_PRECONDITION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_PRECONDITION, DfsmAstPreconditionClass))

typedef struct _DfsmAstPreconditionPrivate	DfsmAstPreconditionPrivate;

typedef struct {
	DfsmAstNode parent;
	DfsmAstPreconditionPrivate *priv;
} DfsmAstPrecondition;

typedef struct {
	DfsmAstNodeClass parent;
} DfsmAstPreconditionClass;

GType dfsm_ast_precondition_get_type (void) G_GNUC_CONST;

gboolean dfsm_ast_precondition_check_is_satisfied (DfsmAstPrecondition *precondition, DfsmEnvironment *environment);
void dfsm_ast_precondition_throw_error (DfsmAstPrecondition *self, DfsmOutputSequence *output_sequence);
const gchar *dfsm_ast_precondition_get_error_name (DfsmAstPrecondition *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_AST_PRECONDITION_H */
