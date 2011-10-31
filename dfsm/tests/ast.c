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

#include <dfsm/dfsm.h>

static void
test_ast_single_object (void)
{
	GPtrArray *object_array;
	GError *error = NULL;

	object_array = dfsm_bison_parse (
		"object at /org/freedesktop/Telepathy/ConnectionManager/gabble implements org.freedesktop.Telepathy.ConnectionManager {\n"
			"data {\n"
				"_ConnectionObjectPath : o = \"TODO\";\n"
				"_ConnectionBusName : s = \"foobar\";\n"
				"_Params : a(susv) = [\n"
					"(\"foo\", 15, \"bar\", \"baz\"),"
					"(\"foo\", 15, \"bar\", \"baz\"),"
				"]\n"
			"}\n"
			"\n"
			"states {\n"
				"Main\n"
			"}\n"
			"\n"
			"transition from Main to Main on method GetParameters {\n"
				"precondition throwing NotImplemented { Protocol in keys(object.Protocols) }\n"
				"\n"
				"reply object._Params\n"
			"}\n"
		"}", &error);

	g_assert_no_error (error);
	g_assert (object_array != NULL);

	if (object_array != NULL) {
		g_ptr_array_unref (object_array);
	}
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/ast/single-object", test_ast_single_object);

	return g_test_run ();
}
