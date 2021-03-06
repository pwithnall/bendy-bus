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

/**
 * SECTION:dfsm-utils
 * @short_description: language utilities
 * @stability: Unstable
 * @include: dfsm/dfsm-utils.h
 *
 * Utility functions used inside and outside the library.
 */

#include <glib.h>

#ifndef DFSM_UTILS_H
#define DFSM_UTILS_H

G_BEGIN_DECLS

gboolean dfsm_is_variable_name (const gchar *variable_name) G_GNUC_PURE;
gboolean dfsm_is_state_name (const gchar *state_name) G_GNUC_PURE;
gboolean dfsm_is_function_name (const gchar *function_name) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_UTILS_H */
