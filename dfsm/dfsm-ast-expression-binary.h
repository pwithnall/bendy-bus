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

#ifndef DFSM_AST_EXPRESSION_BINARY_H
#define DFSM_AST_EXPRESSION_BINARY_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-expression.h>

G_BEGIN_DECLS

/**
 * DfsmAstExpressionBinaryType:
 * @DFSM_AST_EXPRESSION_BINARY_TIMES: a numeric multiplication
 * @DFSM_AST_EXPRESSION_BINARY_DIVIDE: a numeric division
 * @DFSM_AST_EXPRESSION_BINARY_MODULUS: a numeric modulus
 * @DFSM_AST_EXPRESSION_BINARY_PLUS: a numeric addition
 * @DFSM_AST_EXPRESSION_BINARY_MINUS: a numeric subtraction
 * @DFSM_AST_EXPRESSION_BINARY_LT: a numeric less-than comparison
 * @DFSM_AST_EXPRESSION_BINARY_LTE: a numeric less-than-or-equal-to comparison
 * @DFSM_AST_EXPRESSION_BINARY_GT: a numeric greater-than comparison
 * @DFSM_AST_EXPRESSION_BINARY_GTE: a numeric greater-than-or-equal-to comparison
 * @DFSM_AST_EXPRESSION_BINARY_EQ: an equality comparison (valid for all types)
 * @DFSM_AST_EXPRESSION_BINARY_NEQ: an inequality comparison (valid for all types)
 * @DFSM_AST_EXPRESSION_BINARY_AND: a Boolean conjunction
 * @DFSM_AST_EXPRESSION_BINARY_OR: a Boolean disjunction
 *
 * Types of expression defined in terms of a pair of child nodes.
 */
typedef enum {
	DFSM_AST_EXPRESSION_BINARY_TIMES,
	DFSM_AST_EXPRESSION_BINARY_DIVIDE,
	DFSM_AST_EXPRESSION_BINARY_MODULUS,
	DFSM_AST_EXPRESSION_BINARY_PLUS,
	DFSM_AST_EXPRESSION_BINARY_MINUS,
	DFSM_AST_EXPRESSION_BINARY_LT,
	DFSM_AST_EXPRESSION_BINARY_LTE,
	DFSM_AST_EXPRESSION_BINARY_GT,
	DFSM_AST_EXPRESSION_BINARY_GTE,
	DFSM_AST_EXPRESSION_BINARY_EQ,
	DFSM_AST_EXPRESSION_BINARY_NEQ,
	DFSM_AST_EXPRESSION_BINARY_AND,
	DFSM_AST_EXPRESSION_BINARY_OR,
} DfsmAstExpressionBinaryType;

#define DFSM_TYPE_AST_EXPRESSION_BINARY		(dfsm_ast_expression_binary_get_type ())
#define DFSM_AST_EXPRESSION_BINARY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_EXPRESSION_BINARY, DfsmAstExpressionBinary))
#define DFSM_AST_EXPRESSION_BINARY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_EXPRESSION_BINARY, DfsmAstExpressionBinaryClass))
#define DFSM_IS_AST_EXPRESSION_BINARY(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_EXPRESSION_BINARY))
#define DFSM_IS_AST_EXPRESSION_BINARY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_EXPRESSION_BINARY))
#define DFSM_AST_EXPRESSION_BINARY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_EXPRESSION_BINARY, DfsmAstExpressionBinaryClass))

typedef struct _DfsmAstExpressionBinaryPrivate	DfsmAstExpressionBinaryPrivate;

typedef struct {
	DfsmAstExpression parent;
	DfsmAstExpressionBinaryPrivate *priv;
} DfsmAstExpressionBinary;

typedef struct {
	DfsmAstExpressionClass parent;
} DfsmAstExpressionBinaryClass;

GType dfsm_ast_expression_binary_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !DFSM_AST_EXPRESSION_BINARY_H */
