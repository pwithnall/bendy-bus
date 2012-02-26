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
#include <dfsm/dfsm.h>

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

G_BEGIN_DECLS

gchar *load_test_file (const gchar *filename) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GVariant *new_unary_tuple (GVariant *element) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
guint get_counter_from_environment (DfsmEnvironment *environment, const gchar *counter_name);

G_END_DECLS

#endif /* !TEST_UTILS_H */
