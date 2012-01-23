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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "dfsm-utils.h"

#ifndef DFSM_ENVIRONMENT_H
#define DFSM_ENVIRONMENT_H

G_BEGIN_DECLS

typedef enum {
	DFSM_VARIABLE_SCOPE_LOCAL,
	DFSM_VARIABLE_SCOPE_OBJECT,
} DfsmVariableScope;

#define DFSM_TYPE_ENVIRONMENT		(dfsm_environment_get_type ())
#define DFSM_ENVIRONMENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_ENVIRONMENT, DfsmEnvironment))
#define DFSM_ENVIRONMENT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_ENVIRONMENT, DfsmEnvironmentClass))
#define DFSM_IS_ENVIRONMENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_ENVIRONMENT))
#define DFSM_IS_ENVIRONMENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_ENVIRONMENT))
#define DFSM_ENVIRONMENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_ENVIRONMENT, DfsmEnvironmentClass))

typedef struct _DfsmEnvironmentPrivate	DfsmEnvironmentPrivate;

typedef struct {
	GObject parent;
	DfsmEnvironmentPrivate *priv;
} DfsmEnvironment;

typedef struct {
	GObjectClass parent;
} DfsmEnvironmentClass;

GType dfsm_environment_get_type (void) G_GNUC_CONST;

gboolean dfsm_environment_has_variable (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name) G_GNUC_PURE;
GVariantType *dfsm_environment_dup_variable_type (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name) DFSM_CONSTRUCTOR;
void dfsm_environment_set_variable_type (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, const GVariantType *new_type);
GVariant *dfsm_environment_dup_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name) DFSM_CONSTRUCTOR;
void dfsm_environment_set_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name, GVariant *new_value);
void dfsm_environment_unset_variable_value (DfsmEnvironment *self, DfsmVariableScope scope, const gchar *variable_name);

void dfsm_environment_save_reset_point (DfsmEnvironment *self);
void dfsm_environment_reset (DfsmEnvironment *self);

/* Break the dependency loop between dfsm-environment.h and dfsm-ast-expression.h. The actual definition's in dfsm-ast-expression.h. */
typedef struct _DfsmAstExpression DfsmAstExpression;

gboolean dfsm_environment_function_exists (const gchar *function_name) G_GNUC_PURE;
GVariantType *dfsm_environment_function_calculate_type (const gchar *function_name, const GVariantType *parameters_type,
                                                        GError **error) DFSM_CONSTRUCTOR;
GVariant *dfsm_environment_function_evaluate (const gchar *function_name, DfsmAstExpression *parameters_expression,
                                              DfsmEnvironment *environment) DFSM_CONSTRUCTOR;

GPtrArray/*<GDBusInterfaceInfo>*/ *dfsm_environment_get_interfaces (DfsmEnvironment *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_ENVIRONMENT_H */
