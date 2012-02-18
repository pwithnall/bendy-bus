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

#ifndef DFSM_AST_STATEMENT_H
#define DFSM_AST_STATEMENT_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-node.h>
#include <dfsm/dfsm-environment.h>
#include <dfsm/dfsm-output-sequence.h>

G_BEGIN_DECLS

#define DFSM_TYPE_AST_STATEMENT		(dfsm_ast_statement_get_type ())
#define DFSM_AST_STATEMENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_STATEMENT, DfsmAstStatement))
#define DFSM_AST_STATEMENT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_STATEMENT, DfsmAstStatementClass))
#define DFSM_IS_AST_STATEMENT(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_STATEMENT))
#define DFSM_IS_AST_STATEMENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_STATEMENT))
#define DFSM_AST_STATEMENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_STATEMENT, DfsmAstStatementClass))

typedef struct _DfsmAstStatementPrivate	DfsmAstStatementPrivate;

/**
 * DfsmAstStatement:
 *
 * All the fields in the #DfsmAstStatement structure are private and should never be accessed directly.
 */
typedef struct {
	DfsmAstNode parent;
	DfsmAstStatementPrivate *priv;
} DfsmAstStatement;

/**
 * DfsmAstStatementClass:
 * @execute: executes the statement and all its children, appending any state-changing operations to the @output_sequence for later output
 *
 * Class structure for #DfsmAstStatement.
 */
typedef struct {
	/*< private >*/
	DfsmAstNodeClass parent;

	/*< public >*/
	/* Virtual methods */
	void (*execute) (DfsmAstStatement *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence);
} DfsmAstStatementClass;

GType dfsm_ast_statement_get_type (void) G_GNUC_CONST;

void dfsm_ast_statement_execute (DfsmAstStatement *self, DfsmEnvironment *environment, DfsmOutputSequence *output_sequence);

G_END_DECLS

#endif /* !DFSM_AST_STATEMENT_H */
