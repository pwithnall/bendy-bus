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

#ifndef DFSM_AST_EXPRESSION_H
#define DFSM_AST_EXPRESSION_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-node.h>
#include <dfsm/dfsm-environment.h>

G_BEGIN_DECLS

#define DFSM_TYPE_AST_EXPRESSION		(dfsm_ast_expression_get_type ())
#define DFSM_AST_EXPRESSION(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_EXPRESSION, DfsmAstExpression))
#define DFSM_AST_EXPRESSION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_EXPRESSION, DfsmAstExpressionClass))
#define DFSM_IS_AST_EXPRESSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_EXPRESSION))
#define DFSM_IS_AST_EXPRESSION_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_EXPRESSION))
#define DFSM_AST_EXPRESSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_EXPRESSION, DfsmAstExpressionClass))

typedef struct _DfsmAstExpression		DfsmAstExpression;
typedef struct _DfsmAstExpressionPrivate	DfsmAstExpressionPrivate;

struct _DfsmAstExpression {
	DfsmAstNode parent;
	DfsmAstExpressionPrivate *priv;
};

typedef struct {
	DfsmAstNodeClass parent;

	/* Virtual methods */
	GVariantType *(*calculate_type) (DfsmAstExpression *self, DfsmEnvironment *environment);
	GVariant *(*evaluate) (DfsmAstExpression *self, DfsmEnvironment *environment);
	gdouble (*calculate_weight) (DfsmAstExpression *self);
} DfsmAstExpressionClass;

GType dfsm_ast_expression_get_type (void) G_GNUC_CONST;

GVariantType *dfsm_ast_expression_calculate_type (DfsmAstExpression *self, DfsmEnvironment *environment) DFSM_CONSTRUCTOR;
GVariant *dfsm_ast_expression_evaluate (DfsmAstExpression *self, DfsmEnvironment *environment) DFSM_CONSTRUCTOR;
gdouble dfsm_ast_expression_calculate_weight (DfsmAstExpression *self);

G_END_DECLS

#endif /* !DFSM_AST_EXPRESSION_H */
