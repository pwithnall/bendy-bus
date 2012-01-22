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

#ifndef DFSM_AST_EXPRESSION_DATA_STRUCTURE_H
#define DFSM_AST_EXPRESSION_DATA_STRUCTURE_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-data-structure.h>
#include <dfsm/dfsm-ast-expression.h>

G_BEGIN_DECLS

#define DFSM_TYPE_AST_EXPRESSION_DATA_STRUCTURE		(dfsm_ast_expression_data_structure_get_type ())
#define DFSM_AST_EXPRESSION_DATA_STRUCTURE(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_EXPRESSION_DATA_STRUCTURE, DfsmAstExpressionDataStructure))
#define DFSM_AST_EXPRESSION_DATA_STRUCTURE_CLASS(k) \
	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_EXPRESSION_DATA_STRUCTURE, DfsmAstExpressionDataStructureClass))
#define DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_EXPRESSION_DATA_STRUCTURE))
#define DFSM_IS_AST_EXPRESSION_DATA_STRUCTURE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_EXPRESSION_DATA_STRUCTURE))
#define DFSM_AST_EXPRESSION_DATA_STRUCTURE_GET_CLASS(o) \
	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_EXPRESSION_DATA_STRUCTURE, DfsmAstExpressionDataStructureClass))

typedef struct _DfsmAstExpressionDataStructurePrivate	DfsmAstExpressionDataStructurePrivate;

typedef struct {
	DfsmAstExpression parent;
	DfsmAstExpressionDataStructurePrivate *priv;
} DfsmAstExpressionDataStructure;

typedef struct {
	DfsmAstExpressionClass parent;
} DfsmAstExpressionDataStructureClass;

GType dfsm_ast_expression_data_structure_get_type (void) G_GNUC_CONST;

GVariant *dfsm_ast_expression_data_structure_to_variant (DfsmAstExpressionDataStructure *self, DfsmEnvironment *environment) DFSM_CONSTRUCTOR;
void dfsm_ast_expression_data_structure_set_from_variant (DfsmAstExpressionDataStructure *self, DfsmEnvironment *environment, GVariant *new_value);

DfsmAstDataStructure *dfsm_ast_expression_data_structure_get_data_structure (DfsmAstExpressionDataStructure *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_AST_EXPRESSION_DATA_STRUCTURE_H */
