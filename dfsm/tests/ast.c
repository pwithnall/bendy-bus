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

static gchar *
load_test_file (const gchar *filename)
{
	guint8 *contents = NULL;
	gsize length;
	GFile *machine_file;
	GError *error = NULL;

	machine_file = g_file_new_for_path (filename);

	/* Load the file. */
	g_file_load_contents (machine_file, NULL, (gchar**) &contents, &length, NULL, &error);
	g_assert_no_error (error);
	g_assert_cmpuint (length, >, 0);

	g_object_unref (machine_file);

	return (gchar*) contents; /* assume it's text; g_file_load_contents() guarantees it's nul-terminated */
}

static void
test_ast_single_object (void)
{
	gchar *machine_description, *introspection_xml;
	GPtrArray/*<DfsmObject>*/ *object_array;
	GError *error = NULL;

	machine_description = load_test_file ("simple-test.machine");
	introspection_xml = load_test_file ("simple-test.xml");

	object_array = dfsm_object_factory_from_files (machine_description, introspection_xml, &error);

	g_assert_no_error (error);
	g_assert (object_array != NULL);

	if (object_array != NULL) {
		g_ptr_array_unref (object_array);
	}

	g_free (introspection_xml);
	g_free (machine_description);
}

int
main (int argc, char *argv[])
{
	g_type_init ();
#if !GLIB_CHECK_VERSION (2, 31, 0)
	g_thread_init (NULL);
#endif
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/ast/single-object", test_ast_single_object);

	return g_test_run ();
}
