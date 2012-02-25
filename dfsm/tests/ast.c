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
test_machine_description (const gchar *machine_description_filename, const gchar *introspection_xml_filename)
{
	gchar *machine_description, *introspection_xml;
	GPtrArray/*<DfsmObject>*/ *object_array;
	GError *error = NULL;

	machine_description = load_test_file (machine_description_filename);
	introspection_xml = load_test_file (introspection_xml_filename);

	object_array = dfsm_object_factory_from_files (machine_description, introspection_xml, &error);

	g_assert_no_error (error);
	g_assert (object_array != NULL);

	if (object_array != NULL) {
		g_ptr_array_unref (object_array);
	}

	g_free (introspection_xml);
	g_free (machine_description);
}

static void
test_ast_single_object (void)
{
	test_machine_description ("simple-test.machine", "simple-test.xml");
}

static GPtrArray/*<DfsmObject>*/ *
test_machine_description_transition_snippet (const gchar *snippet, GError **error)
{
	gchar *machine_description, *introspection_xml;
	GPtrArray/*<DfsmObject>*/ *object_array;
	const gchar *introspection_xml_filename = "simple-test.xml";

	machine_description = g_strdup_printf (
		"object at /uk/ac/cam/cl/DBusSimulator/ParserTest implements uk.ac.cam.cl.DBusSimulator.SimpleTest {"
			"data {"
				"ArbitraryProperty = \"foo\";"
				"EmptyString = \"\";"
				"SingleCharacter = \"a\";"
				"SingleUnicodeCharacter = \"ö\";"
				"NonEmptyString = \"hello world\";"
				"UnicodeString = \"hállö wèrlđ\";"
				"TypeSignature = @av [];"
			"}"
			"states {"
				"Main;"
				"NotMain;"
			"}"
			"%s"
		"}", snippet);
	introspection_xml = load_test_file (introspection_xml_filename);

	object_array = dfsm_object_factory_from_files (machine_description, introspection_xml, error);

	g_free (introspection_xml);
	g_free (machine_description);

	return object_array;
}

#define ASSERT_TRANSITION_PARSES(Snippet) G_STMT_START { \
	object_array = test_machine_description_transition_snippet ((Snippet), &error); \
	\
	g_assert_no_error (error); \
	g_assert (object_array != NULL); \
	\
	g_clear_error (&error); \
	if (object_array != NULL) { \
		g_ptr_array_unref (object_array); \
	} \
} G_STMT_END

static void
test_ast_parser (void)
{
	GPtrArray/*<DfsmObject>*/ *object_array = NULL;
	GError *error = NULL;
	
	/* Random triggering, inside transitions, signal emissions. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { emit CounterSignal (1u); }");

	/* Method triggering, from…to transitions, replies. */
	ASSERT_TRANSITION_PARSES ("transition from Main to Main on method TwoStateEcho { reply (\"baz\"); }");

	/* Throwing errors. */
	ASSERT_TRANSITION_PARSES ("transition from Main to Main on method TwoStateEcho { throw RandomError; }");

	/* Property triggering. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on property ArbitraryProperty { emit CounterSignal (1u); }");

	/* Preconditions, -> operator, == operator, != operator. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"precondition { object->ArbitraryProperty == \"foo\" }"
		"precondition throwing RandomError { object->ArbitraryProperty != \"foo\" }"
		"emit SingleStateSignal (\"\");"
	"}");

	/* Arithmetic operators. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { emit CounterSignal (1u * 1u / 1u + 1u - 1u % 1u); }");

	/* Operator precedence. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { emit CounterSignal (⟨1u * 1u⟩ / ⟨1u + 1u⟩ - 1u % 1u); }");

	/* Boolean operators. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"precondition { !false == true && false || true }"
		"emit SingleStateSignal (\"\");"
	"}");

	/* Numeric comparisons. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"precondition { 0.5d <~ 0.0d || 0.5d <= 0.0d || 0.5d >= 0.0d || 0.5d ~> 0.0d }"
		"emit SingleStateSignal (\"\");"
	"}");

	/* Arrays, variants, dicts, structs. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"object->TypeSignature = [<1u>];"
		"object->TypeSignature = [<{ \"foo\" : \"bar\" }>];"
		"object->TypeSignature = [<( \"foo\", \"bar\" )>];"
	"}");

	/* Fuzzing. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { object->TypeSignature = @av []?; }");

	/* Transition state lists. */
	ASSERT_TRANSITION_PARSES ("transition inside Main, inside NotMain on random { emit SingleStateSignal (\"\"); }");

	/* Multiple assignment. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { (object->UnicodeString, object->NonEmptyString) = (\"…\", \"Test string\"); }");
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
	g_test_add_func ("/ast/parser", test_ast_parser);

	return g_test_run ();
}
