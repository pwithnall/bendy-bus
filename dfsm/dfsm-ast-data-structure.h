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

/**
 * DfsmAstDictionaryEntry:
 * @key: expression giving the dictionary entry's key
 * @value: expression giving the dictionary entry's value
 *
 * A single dictionary entry, mapping the given @key to the given @value.
 */
typedef struct {
	DfsmAstExpression *key;
	DfsmAstExpression *value;
} DfsmAstDictionaryEntry;

DfsmAstDictionaryEntry *dfsm_ast_dictionary_entry_new (DfsmAstExpression *key, DfsmAstExpression *value) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
void dfsm_ast_dictionary_entry_free (DfsmAstDictionaryEntry *entry);

#define DFSM_TYPE_AST_DATA_STRUCTURE		(dfsm_ast_data_structure_get_type ())
#define DFSM_AST_DATA_STRUCTURE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructure))
#define DFSM_AST_DATA_STRUCTURE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructureClass))
#define DFSM_IS_AST_DATA_STRUCTURE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_DATA_STRUCTURE))
#define DFSM_IS_AST_DATA_STRUCTURE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_DATA_STRUCTURE))
#define DFSM_AST_DATA_STRUCTURE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_DATA_STRUCTURE, DfsmAstDataStructureClass))

typedef struct _DfsmAstDataStructurePrivate	DfsmAstDataStructurePrivate;

/**
 * DfsmAstDataStructure:
 *
 * All the fields in the #DfsmAstDataStructure structure are private and should never be accessed directly.
 */
typedef struct {
	DfsmAstNode parent;
	DfsmAstDataStructurePrivate *priv;
} DfsmAstDataStructure;

/**
 * DfsmAstDataStructureClass:
 *
 * All the fields in the #DfsmAstDataStructureClass structure are private and should never be accessed directly.
 */
typedef struct {
	/*< private >*/
	DfsmAstNodeClass parent;
} DfsmAstDataStructureClass;

GType dfsm_ast_data_structure_get_type (void) G_GNUC_CONST;

gdouble dfsm_ast_data_structure_get_weight (DfsmAstDataStructure *self) G_GNUC_PURE;
const gchar *dfsm_ast_data_structure_get_nickname (DfsmAstDataStructure *self) G_GNUC_PURE;

GVariantType *dfsm_ast_data_structure_calculate_type (DfsmAstDataStructure *self, DfsmEnvironment *environment) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

GVariant *dfsm_ast_data_structure_to_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
void dfsm_ast_data_structure_set_from_variant (DfsmAstDataStructure *self, DfsmEnvironment *environment, GVariant *new_value);

gboolean dfsm_ast_data_structure_is_variable (DfsmAstDataStructure *self);

G_END_DECLS

#endif /* !DFSM_AST_DATA_STRUCTURE_H */
