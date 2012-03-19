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

#include "test-output-sequence.h"
#include "test-utils.h"

static void
test_machine_description (const gchar *machine_description_filename, const gchar *introspection_xml_filename)
{
	gchar *machine_description, *introspection_xml;
	GPtrArray/*<DfsmObject>*/ *object_array;
	GError *error = NULL;

	machine_description = load_test_file (machine_description_filename);
	introspection_xml = load_test_file (introspection_xml_filename);

	object_array = dfsm_object_factory_from_data (machine_description, introspection_xml, &error);

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
build_machine_description_from_transition_snippet (const gchar *snippet, GError **error)
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
				"Integer = @u 0;"
				"Bool = false;"
			"}"
			"states {"
				"Main;"
				"NotMain;"
			"}"
			"%s"
		"}", snippet);
	introspection_xml = load_test_file (introspection_xml_filename);

	object_array = dfsm_object_factory_from_data (machine_description, introspection_xml, error);

	g_free (introspection_xml);
	g_free (machine_description);

	return object_array;
}

#define ASSERT_TRANSITION_PARSES(Snippet) G_STMT_START { \
	object_array = build_machine_description_from_transition_snippet ((Snippet), &error); \
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
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { emit CounterSignal (1); }");

	/* Method triggering, from…to transitions, replies. */
	ASSERT_TRANSITION_PARSES ("transition from Main to Main on method TwoStateEcho { reply (\"baz\"); }");

	/* Throwing errors. */
	ASSERT_TRANSITION_PARSES ("transition from Main to Main on method TwoStateEcho { throw RandomError; }");

	/* Property triggering. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on property ArbitraryProperty { emit CounterSignal (1); }");

	/* Preconditions, -> operator, == operator, != operator. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on method TwoStateEcho {"
		"precondition { object->ArbitraryProperty == \"foo\" }"
		"precondition throwing RandomError { object->ArbitraryProperty != \"foo\" }"
		"reply (\"\");"
	"}");

	/* Arithmetic operators. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { emit CounterSignal (1 * 1 / 1 + 1 - 1 % 1); }");

	/* Operator precedence. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random { emit CounterSignal (⟨1 * 1⟩ / ⟨1 + 1⟩ - 1 % 1); }");

	/* Boolean operators. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"precondition { !false == true && false || true }"
		"emit SingleStateSignal (\"\");"
	"}");

	/* Numeric comparisons. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"precondition { 0.5 <~ 0.0 || 0.5 <= 0.0 || 0.5 >= 0.0 || 0.5 ~> 0.0 }"
		"emit SingleStateSignal (\"\");"
	"}");

	/* Arrays, variants, dicts, structs. */
	ASSERT_TRANSITION_PARSES ("transition inside Main on random {"
		"object->TypeSignature = [<1>];"
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

#define ASSERT_TRANSITION_FAILS(ErrorCode, Snippet) G_STMT_START { \
	object_array = build_machine_description_from_transition_snippet ((Snippet), &error); \
	\
	g_assert_error (error, DFSM_PARSE_ERROR, DFSM_PARSE_ERROR_##ErrorCode); \
	g_assert (object_array == NULL); \
	\
	g_clear_error (&error); \
	if (object_array != NULL) { \
		g_ptr_array_unref (object_array); \
	} \
} G_STMT_END

static void
test_ast_parser_errors (void)
{
	GPtrArray/*<DfsmObject>*/ *object_array = NULL;
	GError *error = NULL;

	/* Wrong annotations. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Integer = @i 5; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Integer = @u -5; }");

	/* Integers which are too wide. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Bool = @y 1000 == @y 1000; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Bool = @q -50 == @q -50; }");

	/* Reference non-existent variable. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->ArbitraryProperty = fake_variable; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->FakeVariable = \"hello\"; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->ArbitraryProperty = object->FakeVariable; }");

	/* Missing reply/throw statement, or reply/throw statement used incorrectly. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho { object->ArbitraryProperty = \"no reply\"; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { reply (\"bad reply\"); }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { throw BadError; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho { reply (\"good reply\"); throw BadError; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho { reply (\"good reply\"); reply (\"bad reply\"); }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho { throw GoodError; throw BadError; }");

	/* Throw statement: invalid error identifier. */
	ASSERT_TRANSITION_FAILS (SYNTAX, "transition inside Main on method TwoStateEcho { throw \"Invalid identifier\"; }");

	/* Reply statement: incorrect type. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho { reply \"not in a struct\"; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho { reply (false); }");

	/* Emit statement: no data, invalid signal, incorrect type. */
	ASSERT_TRANSITION_FAILS (SYNTAX, "transition inside Main on random { emit SingleStateSignal; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { emit NonExistentSignal (true); }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { emit SingleStateSignal (@u 5); }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { emit SingleStateSignal \"not in a struct\"; }");

	/* Assignments: type mismatches, assignments to non-variables and structures of non-variables. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->ArbitraryProperty = false; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { false = true; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { (object->ArbitraryProperty, \"not a variable\") = (\"\", \"\"); }");

	/* Binary expression: type mismatches. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Integer = @u 5 + false; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Bool = \"what\" <= false; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Bool = \"/obj/path\" == @o \"/obj/path\"; }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Bool = true && 1; }");

	/* Unary expression: type mismatches. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Bool = !0; }");

	/* Function call: invalid function name, parameter type mismatch, return type mismatch. */
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Integer = makeMeAnInt (\"shan't\"); }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->ArbitraryProperty = stringJoin (false, @as []); }");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random { object->Integer = stringJoin (\"\", @as []); }");

	/* Precondition: invalid error identifier, type mismatches, throwing error in random transition. */
	ASSERT_TRANSITION_FAILS (SYNTAX, "transition inside Main on method TwoStateEcho {"
		"precondition throwing \"Invalid identifier\" { false }"
		"reply (\"\");"
	"}");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on method TwoStateEcho {"
		"precondition throwing GoodError { \"this isn't a boolean\" }"
		"reply (\"\");"
	"}");
	ASSERT_TRANSITION_FAILS (AST_INVALID, "transition inside Main on random {"
		"precondition throwing GoodError { true }"
		"emit SingleStateSignal (\"\");"
	"}");
}

static void
test_ast_execution_output_sequence (void)
{
	DfsmOutputSequence *output_sequence;
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
	DfsmObject *simulated_object;
	DfsmMachine *machine;
	GVariant *params;
	GError *error = NULL;

	simulated_objects = build_machine_description_from_transition_snippet (
		"transition inside Main on random {"
			"object->Bool = false;" /* no emissions */
		"}"
		"transition inside Main on method SingleStateEcho {"
			"reply (\"test reply\");"
		"}"
		"transition inside Main on method TwoStateEcho {"
			"reply (\"another reply\");"
			"emit SingleStateSignal (\"message\");"
			"emit CounterSignal (1153);"
		"}", &error);
	g_assert_no_error (error);
	g_assert_cmpuint (simulated_objects->len, ==, 1);

	simulated_object = g_ptr_array_index (simulated_objects, 0);
	machine = dfsm_object_get_machine (simulated_object);

	params = g_variant_ref_sink (new_unary_tuple (g_variant_new_string ("test")));

	/* Test no output. */
	output_sequence = test_output_sequence_new (ENTRY_NONE);
	dfsm_machine_make_arbitrary_transition (machine, output_sequence, TRUE);
	dfsm_output_sequence_output (output_sequence, &error);
	g_assert_no_error (error);
	g_object_unref (output_sequence);

	/* Test a single reply. */
	output_sequence = test_output_sequence_new (ENTRY_REPLY, new_unary_tuple (g_variant_new_string ("test reply")), ENTRY_NONE);
	dfsm_machine_call_method (machine, output_sequence, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "SingleStateEcho", params, TRUE);
	dfsm_output_sequence_output (output_sequence, &error);
	g_assert_no_error (error);
	g_object_unref (output_sequence);

	/* Test a reply then two signal emissions. */
	output_sequence = test_output_sequence_new (ENTRY_REPLY, new_unary_tuple (g_variant_new_string ("another reply")),
	                                            ENTRY_EMIT, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "SingleStateSignal",
	                                                        new_unary_tuple (g_variant_new_string ("message")),
	                                            ENTRY_EMIT, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "CounterSignal",
	                                                        new_unary_tuple (g_variant_new_int32 (1153)),
	                                            ENTRY_NONE);
	dfsm_machine_call_method (machine, output_sequence, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "TwoStateEcho", params, TRUE);
	dfsm_output_sequence_output (output_sequence, &error);
	g_assert_no_error (error);
	g_object_unref (output_sequence);

	g_variant_unref (params);
	g_ptr_array_unref (simulated_objects);
}

static void
test_ast_execution_integer_saturation (void)
{
	DfsmOutputSequence *output_sequence;
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
	DfsmObject *simulated_object;
	DfsmMachine *machine;
	DfsmEnvironment *environment;
	GError *error = NULL;

#define ASSERT_ARITHMETIC_EXPRESSION(ARITHMETIC) G_STMT_START { \
	simulated_objects = build_machine_description_from_transition_snippet ( \
		"transition inside Main on random {" \
			"precondition { " ARITHMETIC " }" \
			"object->Integer = object->Integer + @u 1;" /* count successful executions */ \
		"}", &error); \
	g_assert_no_error (error); \
	g_assert_cmpuint (simulated_objects->len, ==, 1); \
	\
	simulated_object = g_ptr_array_index (simulated_objects, 0); \
	machine = dfsm_object_get_machine (simulated_object); \
	environment = dfsm_machine_get_environment (machine); \
	\
	output_sequence = test_output_sequence_new (ENTRY_NONE); \
	dfsm_machine_make_arbitrary_transition (machine, output_sequence, TRUE); \
	g_object_unref (output_sequence); \
	\
	g_assert_cmpuint (get_counter_from_environment (environment, "Integer"), ==, 1); \
	\
	g_ptr_array_unref (simulated_objects); \
} G_STMT_END

	/* Addition. */
	ASSERT_ARITHMETIC_EXPRESSION ("@y 254 + @y 0 == @y 254");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 254 + @y 1 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 254 + @y 2 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 + @y 0 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 + @y 1 == @y 255");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32766 + @n 0 == @n 32766");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32766 + @n 1 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32766 + @n 2 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 + @n 0 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 + @n 1 == @n 32767");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32767 + @n -0 == @n -32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32767 + @n -1 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32767 + @n -2 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 + @n -0 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 + @n -1 == @n -32768");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 + @n 32767 == @n -1");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 + @n -32768 == @n -1");

	/* Subtraction. */
	ASSERT_ARITHMETIC_EXPRESSION ("@y 1 - @y 0 == @y 1");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 1 - @y 1 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 1 - @y 2 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 0 - @y 0 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 0 - @y 1 == @y 0");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32767 - @n 0 == @n -32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32767 - @n 1 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32767 - @n 2 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 - @n 0 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 - @n 1 == @n -32768");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32766 - @n -0 == @n 32766");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32766 - @n -1 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32766 - @n -2 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 - @n -0 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 - @n -1 == @n 32767");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 - @n 32767 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 - @n -32768 == @n 0");

	/* Multiplication. */
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 * @y 0 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 * @y 1 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 * @y 2 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 85 * @y 2 == @y 170");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 85 * @y 3 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 85 * @y 4 == @y 255");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 * @n 0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 * @n 1 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 * @n 2 == @n -32768");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 * @n -0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 * @n -1 == @n -32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 * @n -2 == @n -32768");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 * @n -0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 * @n -1 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 * @n -2 == @n 32767");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 * @n 0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 * @n 1 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 * @n 2 == @n 32767");

	/* Division. */
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 / @y 0 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 / @y 1 == @y 255");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 / @y 2 == @y 127");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 0 / @y 0 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 0 / @y 255 == @y 0");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n 0 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n 1 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n 2 == @n -16384");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n 32767 == @n -1");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n -0 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n -1 == @n -32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n -2 == @n -16383");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n -32768 == @n 0");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n -0 == @n -32768");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n -1 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n -2 == @n 16384");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 / @n -32768 == @n 1");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n 0 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n 1 == @n 32767");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n 2 == @n 16383");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 / @n 32767 == @n 1");

	/* Modulus. */
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 % @y 0 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 % @y 1 == @y 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 % @y 2 == @y 1");
	ASSERT_ARITHMETIC_EXPRESSION ("@y 255 % @y 255 == @y 0");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n 0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n 1 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n 2 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n 3 == @n -2");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n 32767 == @n -1");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n -0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n -1 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n -2 == @n 1");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n -32768 == @n 32767");

	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n -0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n -1 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n -2 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n -3 == @n -2");
	ASSERT_ARITHMETIC_EXPRESSION ("@n -32768 % @n -32767 == @n -1");

	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n 0 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n 1 == @n 0");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n 2 == @n 1");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n 3 == @n 1");
	ASSERT_ARITHMETIC_EXPRESSION ("@n 32767 % @n 32767 == @n 0");

#undef ASSERT_ARITHMETIC_EXPRESSION
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
	g_test_add_func ("/ast/parser/errors", test_ast_parser_errors);
	g_test_add_func ("/ast/execution/output-sequence", test_ast_execution_output_sequence);
	g_test_add_func ("/ast/execution/integer-saturation", test_ast_execution_integer_saturation);

	return g_test_run ();
}
