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

#ifndef DFSM_AST_EXPRESSION_UNARY_H
#define DFSM_AST_EXPRESSION_UNARY_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-expression.h>

G_BEGIN_DECLS

/**
 * DfsmAstExpressionUnaryType:
 * @DFSM_AST_EXPRESSION_UNARY_NOT: a Boolean not (inversion)
 *
 * Types of expression defined in terms of a single child node.
 */
typedef enum {
	DFSM_AST_EXPRESSION_UNARY_NOT,
} DfsmAstExpressionUnaryType;

#define DFSM_TYPE_AST_EXPRESSION_UNARY		(dfsm_ast_expression_unary_get_type ())
#define DFSM_AST_EXPRESSION_UNARY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_EXPRESSION_UNARY, DfsmAstExpressionUnary))
#define DFSM_AST_EXPRESSION_UNARY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_EXPRESSION_UNARY, DfsmAstExpressionUnaryClass))
#define DFSM_IS_AST_EXPRESSION_UNARY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_EXPRESSION_UNARY))
#define DFSM_IS_AST_EXPRESSION_UNARY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_EXPRESSION_UNARY))
#define DFSM_AST_EXPRESSION_UNARY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_EXPRESSION_UNARY, DfsmAstExpressionUnaryClass))

typedef struct _DfsmAstExpressionUnaryPrivate	DfsmAstExpressionUnaryPrivate;

/**
 * DfsmAstExpressionUnary:
 *
 * All the fields in the #DfsmAstExpressionUnary structure are private and should never be accessed directly.
 */
typedef struct {
	DfsmAstExpression parent;
	DfsmAstExpressionUnaryPrivate *priv;
} DfsmAstExpressionUnary;

/**
 * DfsmAstExpressionUnaryClass:
 *
 * All the fields in the #DfsmAstExpressionUnaryClass structure are private and should never be accessed directly.
 */
typedef struct {
	/*< private >*/
	DfsmAstExpressionClass parent;
} DfsmAstExpressionUnaryClass;

GType dfsm_ast_expression_unary_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !DFSM_AST_EXPRESSION_UNARY_H */
