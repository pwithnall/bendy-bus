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

#include <string.h>
#include <glib.h>

#include "dfsm-functions.h"

static const DfsmFunctionInfo _function_info[] = {
	/* Name,	Parameters type,		Return type,			Evaluate func. */
	{ "keys",	G_VARIANT_TYPE_ARRAY,		G_VARIANT_TYPE_ANY,		NULL /* TODO */ }, /* TODO: Not typesafe, but I'm not sure we want polymorphism */
	{ "newObject",	G_VARIANT_TYPE_OBJECT_PATH,	(const GVariantType*) "(os)",	NULL /* TODO */ },
	{ "pairKeys",	(const GVariantType*) "(a?a*)",	(const GVariantType*) "a{?*}",	NULL /* TODO */ },
};

/**
 * dfsm_functions_look_up_function_info:
 * @function_name: name of the function to look up
 *
 * Look up static information about the given @function_name, such as its parameter and return types. If the function isn't known, %NULL will be
 * returned.
 *
 * Return value: (transfer none): information about the function, or %NULL
 */
const DfsmFunctionInfo *
dfsm_functions_look_up_function_info (const gchar *function_name)
{
	guint i;

	g_return_val_if_fail (function_name != NULL && *function_name != '\0', NULL);

	/* Do a linear search for now, since there aren't many functions at all. */
	for (i = 0; i < G_N_ELEMENTS (_function_info); i++) {
		if (strcmp (function_name, _function_info[i].name) == 0) {
			return &_function_info[i];
		}
	}

	return NULL;
}
