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

G_BEGIN_DECLS

#define DFSM_TYPE_AST_OBJECT		(dfsm_ast_object_get_type ())
#define DFSM_AST_OBJECT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_AST_OBJECT, DfsmAstObject))
#define DFSM_AST_OBJECT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_AST_OBJECT, DfsmAstObjectClass))
#define DFSM_IS_AST_OBJECT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_AST_OBJECT))
#define DFSM_IS_AST_OBJECT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_AST_OBJECT))
#define DFSM_AST_OBJECT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_AST_OBJECT, DfsmAstObjectClass))

typedef struct _DfsmAstObjectPrivate	DfsmAstObjectPrivate;

typedef struct {
	DfsmAstNode parent;
	DfsmAstObjectPrivate *priv;
} DfsmAstObject;

typedef struct {
	DfsmAstNodeClass parent;
} DfsmAstObjectClass;

GType dfsm_ast_object_get_type (void) G_GNUC_CONST;

DfsmAstObject *dfsm_ast_object_new (GDBusNodeInfo *dbus_node_info, const gchar *object_path, GPtrArray/*<string>*/ *interface_names,
                                    GPtrArray/*<GHashTable>*/ *data_blocks, GPtrArray/*<GPtrArray>*/ *state_blocks,
                                    GPtrArray/*<DfsmAstTransition>*/ *transition_blocks, GError **error) DFSM_CONSTRUCTOR;

/* TODO: Eliminate these */
DfsmEnvironment *dfsm_ast_object_get_environment (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_ast_object_get_state_names (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<DfsmAstTransition>*/ *dfsm_ast_object_get_transitions (DfsmAstObject *self) G_GNUC_PURE;
const gchar *dfsm_ast_object_get_object_path (DfsmAstObject *self) G_GNUC_PURE;
GPtrArray/*<string>*/ *dfsm_ast_object_get_interface_names (DfsmAstObject *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_AST_OBJECT_H */
