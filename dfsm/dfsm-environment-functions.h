/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * D-Bus Simulator
 * Copyright (C) Philip Withnall 2012 <philip@tecnocode.co.uk>
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

#include <dfsm/dfsm-ast-expression.h>
#include <dfsm/dfsm-environment.h>

#ifndef DFSM_ENVIRONMENT_FUNCTIONS_H
#define DFSM_ENVIRONMENT_FUNCTIONS_H

G_BEGIN_DECLS

gboolean dfsm_environment_function_exists (const gchar *function_name) G_GNUC_PURE;
GVariantType *dfsm_environment_function_calculate_type (const gchar *function_name, const GVariantType *parameters_type,
                                                        GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
GVariant *dfsm_environment_function_evaluate (const gchar *function_name, DfsmAstExpression *parameters_expression,
                                              DfsmEnvironment *environment) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS

#endif /* !DFSM_ENVIRONMENT_FUNCTIONS_H */
