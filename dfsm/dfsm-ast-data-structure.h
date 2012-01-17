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

#ifndef DFSM_AST_DATA_STRUCTURE_H
#define DFSM_AST_DATA_STRUCTURE_H

#include <glib.h>
#include <glib-object.h>

#include <dfsm/dfsm-ast-expression.h>
#include <dfsm/dfsm-ast-node.h>
#include <dfsm/dfsm-utils.h>

G_BEGIN_DECLS

typedef struct {
	DfsmAstExpression *key;
	DfsmAstExpression *value;
} DfsmAstDictionaryEntry;

DfsmAstDictionaryEntry *dfsm_ast_dictionary_entry_new (DfsmAstExpression *key, DfsmAstExpression *value) DFSM_CONSTRUCTOR;
void dfsm_ast_dictionary_entry_free (DfsmAstDictionaryEntry *entry);

/**
 * DfsmAstDataStructureType:
 *
 * TODO
 *
 * Types of data structure (basic or complex).
 */
typedef enum {
	DFSM_AST_DATA_BYTE,
	DFSM_AST_DATA_BOOLEAN,
	DFSM_AST_DATA_INT16,
	DFSM_AST_DATA_UINT16,
	DFSM_AST_DATA_INT32,
	DFSM_AST_DATA_UINT32,
	DFSM_AST_DATA_INT64,
	DFSM_AST_DATA_UINT64,
	DFSM_AST_DATA_DOUBLE,
	DFSM_AST_DATA_STRING,
	DFSM_AST_DATA_OBJECT_PATH,
	DFSM_AST_DATA_SIGNATURE,
	DFSM_AST_DATA_ARRAY,
	DFSM_AST_DATA_STRUCT,
	DFSM_AST_DATA_VARIANT,
	DFSM_AST_DATA_DICT,
	DFSM_AST_DATA_UNIX_FD,
	DFSM_AST_DATA_VARIABLE,
} DfsmAstDataStructureType;

#define DFSM_TYPE_AST_DATA_STRUCTURE		(dfsm_ast_data_structure_get_type ())
#define DFSM_AST_DATA_STRUCTURE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructure))
#define DFSM_AST_DATA_STRUCTURE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructureClass))
#define DFSM_IS_AST_DATA_STRUCTURE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_DATA_STRUCTURE))
#define DFSM_IS_AST_DATA_STRUCTURE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_DATA_STRUCTURE))
#define DFSM_AST_DATA_STRUCTURE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructureClass))

typedef struct _DfsmAstDataStructurePrivate	DfsmAstDataStructurePrivate;

typedef struct {
	DfsmAstNode parent;
	DfsmAstDataStructurePrivate *priv;
} DfsmAstDataStructure;

typedef struct {
	DfsmAstNodeClass parent;
} DfsmAstDataStructureClass;

GType dfsm_ast_data_structure_get_type (void) G_GNUC_CONST;

gdouble dfsm_ast_data_structure_get_weight (DfsmAstDataStructure *self) G_GNUC_PURE;

GVariantType *dfsm_ast_data_structure_calculate_type (DfsmAstDataStructure *self, DfsmEnvironment *environment) DFSM_CONSTRUCTOR;

GVariant *dfsm_ast_data_structure_to_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment, GError **error) DFSM_CONSTRUCTOR;
void dfsm_ast_data_structure_set_from_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment, GVariant *new_value, GError **error);

G_END_DECLS

#endif /* !DFSM_AST_DATA_STRUCTURE_H */
