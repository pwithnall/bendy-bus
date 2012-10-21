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

#include "config.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <dfsm/dfsm.h>

enum StatusCodes {
	STATUS_SUCCESS = 0,
	STATUS_INVALID_OPTIONS = 1,
	STATUS_UNREADABLE_FILE = 2,
	STATUS_INVALID_CODE = 3,
	STATUS_UNREACHABLE_STATES = 4,
};

static void
print_help_text (GOptionContext *context)
{
	gchar *help_text;

	help_text = g_option_context_get_help (context, TRUE, NULL);
	puts (help_text);
	g_free (help_text);
}

int
main (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	const gchar *simulation_filename, *introspection_filename;
	gchar *simulation_code, *introspection_xml;
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
	guint i;
	gboolean found_unreachable_states = FALSE;

	/* Set up localisation. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif
	g_set_application_name (_("D-Bus Simulator Lint"));

	/* Parse command line options */
	context = g_option_context_new (_("[simulation code file] [introspection XML file]"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, _("Checks the FSM simulation code for a D-Bus client–server conversation simulation."));

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_printerr (_("Error parsing command line options: %s"), error->message);
		g_printerr ("\n");

		print_help_text (context);

		g_error_free (error);
		g_option_context_free (context);

		exit (STATUS_INVALID_OPTIONS);
	}

	/* Extract the simulation and the introspection filenames. */
	if (argc < 3) {
		g_printerr (_("Error parsing command line options: %s"), _("Simulation and introspection filenames must be provided"));
		g_printerr ("\n");

		print_help_text (context);

		g_option_context_free (context);

		exit (STATUS_INVALID_OPTIONS);
	}

	simulation_filename = argv[1];
	introspection_filename = argv[2];

	g_option_context_free (context);

	/* Load the files. */
	g_file_get_contents (simulation_filename, &simulation_code, NULL, &error);

	if (error != NULL) {
		g_printerr (_("Error loading simulation code from file ‘%s’: %s"), simulation_filename, error->message);
		g_printerr ("\n");

		g_error_free (error);

		exit (STATUS_UNREADABLE_FILE);
	}

	g_file_get_contents (introspection_filename, &introspection_xml, NULL, &error);

	if (error != NULL) {
		g_printerr (_("Error loading introspection XML from file ‘%s’: %s"), introspection_filename, error->message);
		g_printerr ("\n");

		g_error_free (error);
		g_free (simulation_code);

		exit (STATUS_UNREADABLE_FILE);
	}

	/* Build the DfsmObjects and thus check the simulation code. */
	simulated_objects = dfsm_object_factory_from_data (simulation_code, introspection_xml, &error);

	g_free (introspection_xml);
	g_free (simulation_code);

	if (error != NULL) {
		g_printerr (_("Error creating simulated DFSMs: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);

		exit (STATUS_INVALID_CODE);
	}

	/* Check the reachability of all of the states in each object. */
	for (i = 0; i < simulated_objects->len; i++) {
		DfsmObject *simulated_object;
		DfsmMachine *machine;
		GArray/*<DfsmStateReachability>*/ *reachability;
		DfsmMachineStateNumber state;

		simulated_object = DFSM_OBJECT (g_ptr_array_index (simulated_objects, i));
		machine = dfsm_object_get_machine (simulated_object);
		reachability = dfsm_machine_calculate_state_reachability (machine);

		for (state = 0; state < reachability->len; state++) {
			switch (g_array_index (reachability, DfsmStateReachability, state)) {
				case DFSM_STATE_UNREACHABLE:
					g_printerr (_("State ‘%s’ of object ‘%s’ is unreachable."),
					            dfsm_machine_get_state_name (machine, state),
					            dfsm_object_get_object_path (simulated_object));
					g_printerr ("\n");

					/* Note the error, but continue so that we detect any other unreachable states. */
					found_unreachable_states = TRUE;
					break;
				case DFSM_STATE_POSSIBLY_REACHABLE:
				case DFSM_STATE_REACHABLE:
					/* Nothing to do. */
					break;
				default:
					g_assert_not_reached ();
			}
		}

		g_array_unref (reachability);
	}

	g_ptr_array_unref (simulated_objects);

	/* Did we find at least one unreachable state? Yes? Shame. */
	if (found_unreachable_states == TRUE) {
		return STATUS_UNREACHABLE_STATES;
	}

	return STATUS_SUCCESS;
}
