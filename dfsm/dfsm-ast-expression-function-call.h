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

#ifndef DFSM_AST_EXPRESSION_FUNCTION_CALL_H
#define DFSM_AST_EXPRESSION_FUNCTION_CALL_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-expression.h>
#include <dfsm/dfsm-environment.h>

G_BEGIN_DECLS

#define DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL		(dfsm_ast_expression_function_call_get_type ())
#define DFSM_AST_EXPRESSION_FUNCTION_CALL(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL, DfsmAstExpressionFunctionCall))
#define DFSM_AST_EXPRESSION_FUNCTION_CALL_CLASS(k) \
	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL, DfsmAstExpressionFunctionCallClass))
#define DFSM_IS_AST_EXPRESSION_FUNCTION_CALL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL))
#define DFSM_IS_AST_EXPRESSION_FUNCTION_CALL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL))
#define DFSM_AST_EXPRESSION_FUNCTION_CALL_GET_CLASS(o) \
	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_EXPRESSION_FUNCTION_CALL, DfsmAstExpressionFunctionCallClass))

typedef struct _DfsmAstExpressionFunctionCallPrivate	DfsmAstExpressionFunctionCallPrivate;

typedef struct {
	DfsmAstExpression parent;
	DfsmAstExpressionFunctionCallPrivate *priv;
} DfsmAstExpressionFunctionCall;

typedef struct {
	DfsmAstExpressionClass parent;
} DfsmAstExpressionFunctionCallClass;

GType dfsm_ast_expression_function_call_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !DFSM_AST_EXPRESSION_FUNCTION_CALL_H */
