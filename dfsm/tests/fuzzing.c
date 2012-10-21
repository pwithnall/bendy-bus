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

#include <string.h>
#include <dfsm/dfsm.h>

#include "test-utils.h"

#define TEST_COUNT 10000
#define DELTA 0.1

static DfsmAstDataStructure *
build_data_structure_from_snippet (const gchar *snippet, DfsmEnvironment **environment)
{
	GPtrArray/*<DfsmAstObject>*/ *ast_objects;
	gchar *simulation_description, *introspection_xml;
	DfsmAstObjectTransition *object_transition;
	GPtrArray/*<DfsmAstObjectTransition>*/ *transitions;
	GPtrArray/*<DfsmAstStatement>*/ *statements;
	DfsmAstExpression *expression;
	DfsmAstDataStructure *data_structure;
	GError *error = NULL;

	simulation_description = g_strdup_printf ("object at /uk/ac/cam/cl/DBusSimulator/ParserTest implements uk.ac.cam.cl.DBusSimulator.SimpleTest {"
			"data {"
				"ArbitraryProperty = \"foo\";"
				"Variant = <false>;"
			"}"
			"states {"
				"Main;"
			"}"
			"transition inside Main on random {"
				"object->Variant = <%s>;"
			"}"
		"}", snippet);
	introspection_xml = load_test_file ("simple-test.xml");

	ast_objects = dfsm_object_factory_asts_from_data (simulation_description, introspection_xml, &error);
	g_assert_no_error (error);
	g_assert (ast_objects != NULL);
	g_assert_cmpuint (ast_objects->len, ==, 1);

	/* Extract the environment. */
	*environment = g_object_ref (dfsm_ast_object_get_environment (g_ptr_array_index (ast_objects, 0)));

	/* Extract the data structure we're interested in. */
	transitions = dfsm_ast_object_get_transitions (g_ptr_array_index (ast_objects, 0));
	g_assert (transitions != NULL);
	g_assert_cmpuint (transitions->len, ==, 1);

	object_transition = (DfsmAstObjectTransition*) g_ptr_array_index (transitions, 0);
	statements = dfsm_ast_transition_get_statements (object_transition->transition);
	g_assert (statements != NULL);
	g_assert_cmpuint (statements->len, ==, 1);

	expression = dfsm_ast_statement_assignment_get_expression (g_ptr_array_index (statements, 0));
	g_assert (expression != NULL);

	/* Get our return value. */
	data_structure = g_object_ref (dfsm_ast_expression_data_structure_get_data_structure (DFSM_AST_EXPRESSION_DATA_STRUCTURE (expression)));

	g_free (introspection_xml);
	g_free (simulation_description);

	g_ptr_array_unref (ast_objects);

	return data_structure;
}

static void
test_fuzzing_integers_unsigned (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint bins[4] = { 0, };
	guint i;
	enum {
		SMALL_RANGE = 0,
		DEFAULT,
		BOUNDARY,
		LARGE_RANGE,
	};

	data_structure = build_data_structure_from_snippet ("@u 666?", &environment);

	/* Evaluate the expression a number of times, building up an approximation of the probability distribution used for fuzzing integers. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		guint val;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_uint32 (_variant);
		g_variant_unref (_variant);
		g_variant_unref (variant);

		if (val <= 10) {
			/* Small range. */
			bins[SMALL_RANGE]++;
		} else if (val == 666) {
			/* Default value. */
			bins[DEFAULT]++;
		} else if (val == 0 || val == G_MAXUINT32) {
			/* Boundary. */
			bins[BOUNDARY]++;
		} else {
			/* Large range. */
			bins[LARGE_RANGE]++;
		}
	}

	g_object_unref (data_structure);
	g_object_unref (environment);

	/* Check the distribution is roughly as expected. */
	g_assert_cmpuint (bins[SMALL_RANGE], >=, TEST_COUNT * (0.3 - DELTA));
	g_assert_cmpuint (bins[SMALL_RANGE], <=, TEST_COUNT * (0.3 + DELTA));
	g_assert_cmpuint (bins[DEFAULT], >=, TEST_COUNT * (0.3 - DELTA));
	g_assert_cmpuint (bins[DEFAULT], <=, TEST_COUNT * (0.3 + DELTA));
	g_assert_cmpuint (bins[BOUNDARY], >=, TEST_COUNT * (0.1 - DELTA));
	g_assert_cmpuint (bins[BOUNDARY], <=, TEST_COUNT * (0.1 + DELTA));
	g_assert_cmpuint (bins[LARGE_RANGE], >=, TEST_COUNT * (0.3 - DELTA));
	g_assert_cmpuint (bins[LARGE_RANGE], <=, TEST_COUNT * (0.3 + DELTA));
}

static void
test_fuzzing_integers_signed (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint bins[4] = { 0, };
	guint i;
	enum {
		SMALL_RANGE = 0,
		DEFAULT,
		BOUNDARY,
		LARGE_RANGE,
	};

	data_structure = build_data_structure_from_snippet ("666?", &environment);

	/* Evaluate the expression a number of times, building up an approximation of the probability distribution used for fuzzing integers. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		gint val;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_int32 (_variant);
		g_variant_unref (_variant);
		g_variant_unref (variant);

		if (val >= -5 && val <= 5) {
			/* Small range. */
			bins[SMALL_RANGE]++;
		} else if (val == 666) {
			/* Default value. */
			bins[DEFAULT]++;
		} else if (val == G_MININT32 || val == G_MAXINT32) {
			/* Boundary. */
			bins[BOUNDARY]++;
		} else {
			/* Large range. */
			bins[LARGE_RANGE]++;
		}
	}

	g_object_unref (data_structure);
	g_object_unref (environment);

	/* Check the distribution is roughly as expected. */
	g_assert_cmpuint (bins[SMALL_RANGE], >=, TEST_COUNT * (0.3 - DELTA));
	g_assert_cmpuint (bins[SMALL_RANGE], <=, TEST_COUNT * (0.3 + DELTA));
	g_assert_cmpuint (bins[DEFAULT], >=, TEST_COUNT * (0.3 - DELTA));
	g_assert_cmpuint (bins[DEFAULT], <=, TEST_COUNT * (0.3 + DELTA));
	g_assert_cmpuint (bins[BOUNDARY], >=, TEST_COUNT * (0.1 - DELTA));
	g_assert_cmpuint (bins[BOUNDARY], <=, TEST_COUNT * (0.1 + DELTA));
	g_assert_cmpuint (bins[LARGE_RANGE], >=, TEST_COUNT * (0.3 - DELTA));
	g_assert_cmpuint (bins[LARGE_RANGE], <=, TEST_COUNT * (0.3 + DELTA));
}

static void
test_fuzzing_booleans (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint bins[2] = { 0, };
	guint i;
	enum {
		DEFAULT = 0,
		FLIPPED,
	};

	data_structure = build_data_structure_from_snippet ("false?", &environment);

	/* Evaluate the expression a number of times, building up an approximation of the probability distribution used for fuzzing booleans. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		gboolean val;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_boolean (_variant);
		g_variant_unref (_variant);
		g_variant_unref (variant);

		if (val == FALSE) {
			/* Default. */
			bins[DEFAULT]++;
		} else {
			/* Flipped. */
			bins[FLIPPED]++;
		}
	}

	g_object_unref (data_structure);
	g_object_unref (environment);

	/* Check the distribution is roughly as expected. */
	g_assert_cmpuint (bins[DEFAULT], >=, TEST_COUNT * (0.6 - DELTA));
	g_assert_cmpuint (bins[DEFAULT], <=, TEST_COUNT * (0.6 + DELTA));
	g_assert_cmpuint (bins[FLIPPED], >=, TEST_COUNT * (0.4 - DELTA));
	g_assert_cmpuint (bins[FLIPPED], <=, TEST_COUNT * (0.4 + DELTA));
}

static void
test_fuzzing_strings (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint i;

	data_structure = build_data_structure_from_snippet ("\"test string, or something\"?", &environment);

	/* Evaluate the expression a number of times, checking whether each one is valid. It's not possible to build up a quantitative measure of
	 * the distribution of different fuzzing methods used when fuzzing strings, since reverse-engineering the fuzzed string to determine the
	 * fuzzing method used is too difficult. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		const gchar *val;
		gsize len;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_string (_variant, &len);

		/* Print out the string for inspection. */
		/*g_message ("%s", val);*/

		/* Check the string. */
		g_assert_cmpuint (strlen (val), ==, len);
		g_assert (g_utf8_validate (val, len, NULL) == TRUE);

		g_variant_unref (_variant);
		g_variant_unref (variant);
	}

	g_object_unref (data_structure);
	g_object_unref (environment);
}

static void
test_fuzzing_object_paths (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint i;

	data_structure = build_data_structure_from_snippet ("@o \"/object/path\"?", &environment);

	/* Evaluate the expression a number of times, checking whether each one is valid. It's not possible to build up a quantitative measure of
	 * the distribution of different fuzzing methods used when fuzzing strings, since reverse-engineering the fuzzed string to determine the
	 * fuzzing method used is too difficult. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		const gchar *val;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_string (_variant, NULL);

		/* Print out the object path for inspection. */
		/*g_message ("%s", val);*/

		/* Check the object path. */
		g_assert (g_variant_is_object_path (val));

		g_variant_unref (_variant);
		g_variant_unref (variant);
	}

	g_object_unref (data_structure);
	g_object_unref (environment);
}

static void
test_fuzzing_signatures (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint i;

	data_structure = build_data_structure_from_snippet ("@g \"a{sv}\"?", &environment);

	/* Evaluate the expression a number of times, checking whether each one is valid. It's not possible to build up a quantitative measure of
	 * the distribution of different fuzzing methods used when fuzzing strings, since reverse-engineering the fuzzed string to determine the
	 * fuzzing method used is too difficult. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		const gchar *val;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_string (_variant, NULL);

		/* Print out the signature for inspection. */
		/*g_message ("%s", val);*/

		/* Check the signature. */
		g_assert (g_variant_is_signature (val));

		g_variant_unref (_variant);
		g_variant_unref (variant);
	}

	g_object_unref (data_structure);
	g_object_unref (environment);
}

static void
test_fuzzing_arrays (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint i;

	data_structure = build_data_structure_from_snippet ("[\"default value 1\", \"default value 2\", \"default value 3\"]?", &environment);

	/* Evaluate the expression a number of times, checking whether each one is valid. It's not possible to build up a quantitative measure of
	 * the distribution of different fuzzing methods used when fuzzing arrays, since reverse-engineering the fuzzed array to determine the
	 * fuzzing method used is too difficult. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		/*gchar *val;*/

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);

		/* Print out the array for inspection. */
		/*val = g_variant_print (_variant, TRUE);
		g_message ("%s", val);
		g_free (val);*/

		/* Check the array length is in [0, 9], since at most each element will have been cloned twice. */
		g_assert_cmpuint (g_variant_n_children (_variant), <=, 9);

		g_variant_unref (_variant);
		g_variant_unref (variant);
	}

	g_object_unref (data_structure);
	g_object_unref (environment);
}

static void
test_fuzzing_dictionaries (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint i;

	data_structure = build_data_structure_from_snippet ("{\"key 1\" : \"value 1\", \"key 2\" : \"value 2\", \"key 3\" : \"value 3\"}?",
	                                                    &environment);

	/* Evaluate the expression a number of times, checking whether each one is valid. It's not possible to build up a quantitative measure of
	 * the distribution of different fuzzing methods used when fuzzing dictionaries, since reverse-engineering the fuzzed dictionary to determine
	 * the fuzzing method used is too difficult. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant;
		/*gchar *val;*/

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);

		/* Print out the dictionary for inspection. */
		/*val = g_variant_print (_variant, TRUE);
		g_message ("%s", val);
		g_free (val);*/

		/* Check the dictionary length is in [0, 9], since at most each entry will have been cloned twice. */
		g_assert_cmpuint (g_variant_n_children (_variant), <=, 9);

		g_variant_unref (_variant);
		g_variant_unref (variant);
	}

	g_object_unref (data_structure);
	g_object_unref (environment);
}

static void
test_fuzzing_variants (void)
{
	DfsmAstDataStructure *data_structure;
	DfsmEnvironment *environment = NULL;
	guint bins[2] = { 0, };
	guint i;
	enum {
		DEFAULT = 0,
		NEW_TYPE,
	};

	data_structure = build_data_structure_from_snippet ("<\"string!\">?", &environment);

	/* Evaluate the expression a number of times, building up an approximation of the probability distribution used for fuzzing variants. */
	for (i = 0; i < TEST_COUNT; i++) {
		GVariant *variant, *_variant, *val;

		variant = dfsm_ast_data_structure_to_variant (data_structure, environment);
		_variant = g_variant_get_variant (variant);
		val = g_variant_get_variant (_variant);
		g_variant_unref (_variant);
		g_variant_unref (variant);

		if (g_variant_type_equal (g_variant_get_type (val), G_VARIANT_TYPE_STRING) == TRUE) {
			/* Default. */
			bins[DEFAULT]++;
		} else {
			/* New type. */
			bins[NEW_TYPE]++;
		}

		g_variant_unref (val);
	}

	g_object_unref (data_structure);
	g_object_unref (environment);

	/* Check the distribution is roughly as expected. */
	g_assert_cmpuint (bins[DEFAULT], >=, TEST_COUNT * (0.8 - DELTA));
	g_assert_cmpuint (bins[DEFAULT], <=, TEST_COUNT * (0.8 + DELTA));
	g_assert_cmpuint (bins[NEW_TYPE], >=, TEST_COUNT * (0.2 - DELTA));
	g_assert_cmpuint (bins[NEW_TYPE], <=, TEST_COUNT * (0.2 + DELTA));
}

int
main (int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif
#if !GLIB_CHECK_VERSION (2, 31, 0)
	g_thread_init (NULL);
#endif
	g_test_init (&argc, &argv, NULL);

	/* Basic types. */
	g_test_add_func ("/fuzzing/integers/unsigned", test_fuzzing_integers_unsigned);
	g_test_add_func ("/fuzzing/integers/signed", test_fuzzing_integers_signed);
	g_test_add_func ("/fuzzing/booleans", test_fuzzing_booleans);
	g_test_add_func ("/fuzzing/strings", test_fuzzing_strings);
	g_test_add_func ("/fuzzing/object-paths", test_fuzzing_object_paths);
	g_test_add_func ("/fuzzing/signatures", test_fuzzing_signatures);

	/* Container types. Note that we don't test fuzzing of tuples/structs, since fuzzing them makes no sense (just fuzz their elements instead). */
	g_test_add_func ("/fuzzing/arrays", test_fuzzing_arrays);
	g_test_add_func ("/fuzzing/dictionaries", test_fuzzing_dictionaries);
	g_test_add_func ("/fuzzing/variants", test_fuzzing_variants);

	return g_test_run ();
}
