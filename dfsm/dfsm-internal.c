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
#include <gio/gio.h>

#include "dfsm-internal.h"

GVariantType *
dfsm_internal_dbus_arg_info_array_to_variant_type (const GDBusArgInfo **args)
{
	GVariantType *variant_type;
	GPtrArray/*<const GVariantType>*/ *args_type_array;
	guint i;

	/* Special-case empty args by returning the unit tuple. */
	if (args == NULL) {
		return g_variant_type_new_tuple (NULL, 0);
	}

	args_type_array = g_ptr_array_new ();

	for (i = 0; args[i] != NULL; i++) {
		/* Treat the signature as a const GVariantType. */
		g_ptr_array_add (args_type_array, (GVariantType*) (args[i]->signature));
	}

	variant_type = g_variant_type_new_tuple ((const GVariantType * const *)args_type_array->pdata, i);

	g_ptr_array_free (args_type_array, TRUE);

	return variant_type;
}
