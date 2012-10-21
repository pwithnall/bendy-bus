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
				"Counter = @u 100;"
				"Random1Counter = @u 0;"
				"Random2Counter = @u 0;"
				"SingleEcho1Counter = @u 0;"
				"SingleEcho2Counter = @u 0;"
				"TwoEchoCounter = @u 0;"
				"ArbitraryPropertySetCounter = @u 0;"
			"}"
			"states {"
				"Main;"
			"}"
			"%s"
		"}", snippet);
	introspection_xml = load_test_file (introspection_xml_filename);

	object_array = dfsm_object_factory_from_data (machine_description, introspection_xml, error);

	g_free (introspection_xml);
	g_free (machine_description);

	return object_array;
}

static void
test_simulation_probabilities (void)
{
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
	DfsmObject *simulated_object;
	DfsmMachine *machine;
	DfsmEnvironment *environment;
	GVariant *params, *val;
	guint i;
	GError *error = NULL;

	#define TEST_COUNT 10000
	#define DELTA 100

	/* We build a simulation with several types of parallelism:
	 *  • Two arbitrarily triggered transitions (Random1 and Random2) which should be chosen between uniformly when a random trigger occurs.
	 *  • Two method triggered transitions (SingleEcho1 and SingleEcho2) which should be chosen between uniformly when SingleStateEcho is called.
	 *  • A single method triggered transition (TwoEcho) which should always be executed when TwoStateEcho is called.
	 *  • A single property triggered transition (ArbitraryPropertySet) which should be executed roughly half of the time it's called, since it's
	 *    predicated on the counter.
	 *
	 * All transitions update counters inside the simulation, which we later check by querying the environment.
	 */
	simulated_objects = build_machine_description_from_transition_snippet (
		"transition Random1 inside Main on random {"
			"object->Counter = object->Counter + @u 1;"
			"object->Random1Counter = object->Random1Counter + @u 1;"
		"}"
		"transition Random2 inside Main on random {"
			"object->Counter = object->Counter - @u 1;"
			"object->Random2Counter = object->Random2Counter + @u 1;"
		"}"
		"transition SingleEcho1 inside Main on method SingleStateEcho {"
			"reply (\"reply\");"
			"object->SingleEcho1Counter = object->SingleEcho1Counter + @u 1;"
		"}"
		"transition SingleEcho2 inside Main on method SingleStateEcho {"
			"reply (\"reply\");"
			"object->SingleEcho2Counter = object->SingleEcho2Counter + @u 1;"
		"}"
		"transition TwoEcho inside Main on method TwoStateEcho {"
			"reply (\"reply\");"
			"object->TwoEchoCounter = object->TwoEchoCounter + @u 1;"
		"}"
		"transition ArbitraryPropertySet inside Main on property ArbitraryProperty {"
			"precondition { object->Counter % @u 2 == @u 0 }"
			"object->ArbitraryProperty = value;"
			"object->ArbitraryPropertySetCounter = object->ArbitraryPropertySetCounter + @u 1;"
		"}", &error);
	g_assert_no_error (error);
	g_assert_cmpuint (simulated_objects->len, ==, 1);

	simulated_object = g_ptr_array_index (simulated_objects, 0);
	machine = dfsm_object_get_machine (simulated_object);

	environment = g_object_ref (dfsm_machine_get_environment (machine));

	params = g_variant_ref_sink (new_unary_tuple (g_variant_new_string ("param")));
	val = g_variant_ref_sink (g_variant_new_string ("value"));

	for (i = 0; i < TEST_COUNT; i++) {
		DfsmOutputSequence *output_sequence;

		/* Make a random transition, call SingleStateEcho, call TwoStateEcho and then set ArbitraryProperty. */
		output_sequence = test_output_sequence_new (ENTRY_NONE);
		dfsm_machine_make_arbitrary_transition (machine, output_sequence, TRUE);
		g_object_unref (output_sequence);

		output_sequence = test_output_sequence_new (ENTRY_REPLY, new_unary_tuple (g_variant_new_string ("reply")), ENTRY_NONE);
		dfsm_machine_call_method (machine, output_sequence, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "SingleStateEcho", params, TRUE);
		g_object_unref (output_sequence);

		output_sequence = test_output_sequence_new (ENTRY_REPLY, new_unary_tuple (g_variant_new_string ("reply")), ENTRY_NONE);
		dfsm_machine_call_method (machine, output_sequence, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "TwoStateEcho", params, TRUE);
		g_object_unref (output_sequence);

		output_sequence = test_output_sequence_new (ENTRY_NONE);
		dfsm_machine_set_property (machine, output_sequence, "uk.ac.cam.cl.DBusSimulator.SimpleTest", "ArbitraryProperty", val, TRUE);
		g_object_unref (output_sequence);
	}

	g_variant_unref (val);
	g_variant_unref (params);
	g_ptr_array_unref (simulated_objects);

	/* Check the counters. We expect:
	 *  • Counter to be near 100, since equal probability transitions incremented and decremented it.
	 *  • Random1Counter + Random2Counter == TEST_COUNT.
	 *  • Random1Counter and Random2Counter to both be roughly TEST_COUNT/2.
	 *  • SingleEcho1Counter + SingleEcho2Counter == TEST_COUNT.
	 *  • SingleEcho1Counter and SingleEcho2Counter to both be roughly TEST_COUNT/2.
	 *  • TwoEchoCounter == TEST_COUNT.
	 *  • ArbitraryPropertySetCounter to be roughly TEST_COUNT/2.
	 */
#define ASSERT_IN_RANGE(CounterName, Expectation, Delta) G_STMT_START { \
		guint __counter = get_counter_from_environment (environment, (CounterName)); \
		g_assert_cmpuint (__counter, >=, (Expectation) - (Delta)); \
		g_assert_cmpuint (__counter, <=, (Expectation) + (Delta)); \
	} G_STMT_END

	ASSERT_IN_RANGE ("Counter", 100, DELTA);
	ASSERT_IN_RANGE ("Random1Counter", TEST_COUNT / 2, DELTA);
	ASSERT_IN_RANGE ("Random2Counter", TEST_COUNT / 2, DELTA);
	ASSERT_IN_RANGE ("SingleEcho1Counter", TEST_COUNT / 2, DELTA);
	ASSERT_IN_RANGE ("SingleEcho2Counter", TEST_COUNT / 2, DELTA);
	ASSERT_IN_RANGE ("TwoEchoCounter", TEST_COUNT, 0);
	ASSERT_IN_RANGE ("ArbitraryPropertySetCounter", TEST_COUNT / 2, DELTA);

	g_assert_cmpuint (get_counter_from_environment (environment, "Random1Counter") +
	                  get_counter_from_environment (environment, "Random2Counter"), ==, TEST_COUNT);
	g_assert_cmpuint (get_counter_from_environment (environment, "SingleEcho1Counter") +
	                  get_counter_from_environment (environment, "SingleEcho2Counter"), ==, TEST_COUNT);

#undef ASSERT_IN_RANGE

	g_object_unref (environment);
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

	g_test_add_func ("/simulation/probabilities", test_simulation_probabilities);

	return g_test_run ();
}
