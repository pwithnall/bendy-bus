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
	STATUS_OBJECT_NOT_FOUND = 4,
	STATUS_IO_ERROR = 5,
};

static const gchar *graph_id = NULL;
static const gchar *object_path = NULL;

const GOptionEntry main_entries[] = {
	{ "graph-id", 'i', 0, G_OPTION_ARG_STRING, &graph_id, N_("ID of the outermost graph block (default: ‘bendy_bus’)"), N_("ID") },
	{ "object-path", 'o', 0, G_OPTION_ARG_STRING, &object_path, N_("Object path of a single object to output (default: output all objects)"),
	  N_("OBJECT PATH") },
	{ NULL },
};

static gchar *
format_transition_label (DfsmAstTransition *transition)
{
	switch (dfsm_ast_transition_get_trigger (transition)) {
		case DFSM_AST_TRANSITION_METHOD_CALL:
			return g_strdup_printf ("%s() (%p)", dfsm_ast_transition_get_trigger_method_name (transition), transition);
		case DFSM_AST_TRANSITION_PROPERTY_SET:
			return g_strdup_printf ("::%s (%p)", dfsm_ast_transition_get_trigger_property_name (transition), transition);
		case DFSM_AST_TRANSITION_ARBITRARY:
			return g_strdup_printf ("random (%p)", transition);
		default:
			g_warning (_("Unrecognized transition trigger type: %u."), dfsm_ast_transition_get_trigger (transition));
			return g_strdup_printf ("%p", transition);
	}
}

static void
ast_object_print_graph (DfsmAstObject *ast_object, GString *output_string)
{
	guint i;
	GPtrArray/*<string>*/ *bus_names, *interface_names, *states;
	GPtrArray/*<DfsmAstObjectTransition>*/ *transitions;

	/* Print out a graph of all the states and transitions. We don't bother to include any of the transitions' code, since that would confuse
	 * things. */
	g_string_append_printf (output_string,
		"subgraph \"cluster_%s\" {\n"
			"\trankdir = LR;\n"
			"\tcolor = black;\n"
			"\tminlen = 5;\n"
			"\tlabel = \"object at %s", dfsm_ast_object_get_object_path (ast_object), dfsm_ast_object_get_object_path (ast_object));

	bus_names = dfsm_ast_object_get_well_known_bus_names (ast_object);

	for (i = 0; i < bus_names->len; i++) {
		g_string_append_printf (output_string, ", %s", (gchar*) g_ptr_array_index (bus_names, i));
	}

	g_string_append (output_string, " implements ");

	interface_names = dfsm_ast_object_get_interface_names (ast_object);

	for (i = 0; i < interface_names->len; i++) {
		if (i > 0) {
			g_string_append (output_string, ", ");
		}

		g_string_append (output_string, g_ptr_array_index (interface_names, i));
	}

	g_string_append (output_string, "\";\n\n");

	/* States */
	states = dfsm_ast_object_get_state_names (ast_object);

	for (i = 0; i < states->len; i++) {
		const gchar *state_name;

		state_name = g_ptr_array_index (states, i);

		if (i == 0) {
			/* Starting state gets a double circle. */
			g_string_append_printf (output_string, "\t\"S_%p_%u\" [shape = doublecircle, label = \"%s\"];\n", ast_object, i, state_name);
		} else {
			/* Other states get tacked on to the end of the normal circle list. */
			g_string_append_printf (output_string, "\t\"S_%p_%u\" [shape = circle, label = \"%s\"];\n", ast_object, i, state_name);
		}
	}

	g_string_append_c (output_string, '\n');

	/* Transitions */
	transitions = dfsm_ast_object_get_transitions (ast_object);

	for (i = 0; i < transitions->len; i++) {
		const DfsmAstObjectTransition *transition;
		gchar *label;
		const gchar *properties;

		transition = g_ptr_array_index (transitions, i);

		/* Vary the edge properties based on the type of transition. */
		switch (dfsm_ast_transition_get_trigger (transition->transition)) {
			case DFSM_AST_TRANSITION_METHOD_CALL:
				properties = ", color = red";
				break;
			case DFSM_AST_TRANSITION_PROPERTY_SET:
				properties = ", color = blue";
				break;
			case DFSM_AST_TRANSITION_ARBITRARY:
				properties = ", color = green";
				break;
			default:
				g_warning (_("Unrecognized transition trigger type: %u."), dfsm_ast_transition_get_trigger (transition->transition));
				properties = "";
				break;
		}

		/* Output the edge. */
		label = format_transition_label (transition->transition);
		g_string_append_printf (output_string, "\tS_%p_%u -> S_%p_%u [label = \"%s\"%s];\n", ast_object, transition->from_state, ast_object,
		                        transition->to_state, label, properties);
		g_free (label);
	}

	g_string_append (output_string, "}\n");
}

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
	guint i;
	GPtrArray/*<DfsmAstObject>*/ *ast_objects;
	GString *graphviz_string;
	gchar *time_str, *command_line;
	GDateTime *date_time;
	gboolean object_found = FALSE;

	/* Set up localisation. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();
	g_set_application_name (_("D-Bus Simulator GraphViz Printer"));

	/* Suppress debug output or it'll end up in the GraphViz code. */
	g_unsetenv ("G_MESSAGES_DEBUG");

	/* Take a copy of the command line to output in the graph comment later on. */
	command_line = g_strjoinv (" ", argv);

	/* Parse command line options */
	context = g_option_context_new (_("[simulation code file] [introspection XML file]"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, _("Outputs GraphViz code for the FSM for a D-Bus client–server conversation simulation."));
	g_option_context_add_main_entries (context, main_entries, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_printerr (_("Error parsing command line options: %s"), error->message);
		g_printerr ("\n");

		print_help_text (context);

		g_error_free (error);
		g_option_context_free (context);
		g_free (command_line);

		exit (STATUS_INVALID_OPTIONS);
	}

	/* Extract the simulation and the introspection filenames. */
	if (argc < 3) {
		g_printerr (_("Error parsing command line options: %s"), _("Simulation and introspection filenames must be provided"));
		g_printerr ("\n");

		print_help_text (context);

		g_option_context_free (context);
		g_free (command_line);

		exit (STATUS_INVALID_OPTIONS);
	}

	simulation_filename = argv[1];
	introspection_filename = argv[2];

	if (graph_id == NULL) {
		graph_id = "bendy_bus"; /* default; if changing this, also change it in main_entries */
	}

	g_option_context_free (context);

	/* Load the files. */
	g_file_get_contents (simulation_filename, &simulation_code, NULL, &error);

	if (error != NULL) {
		g_printerr (_("Error loading simulation code from file ‘%s’: %s"), simulation_filename, error->message);
		g_printerr ("\n");

		g_error_free (error);
		g_free (command_line);

		exit (STATUS_UNREADABLE_FILE);
	}

	g_file_get_contents (introspection_filename, &introspection_xml, NULL, &error);

	if (error != NULL) {
		g_printerr (_("Error loading introspection XML from file ‘%s’: %s"), introspection_filename, error->message);
		g_printerr ("\n");

		g_error_free (error);
		g_free (simulation_code);
		g_free (command_line);

		exit (STATUS_UNREADABLE_FILE);
	}

	/* Build the DfsmObjects from which we can print the GraphViz code. */
	ast_objects = dfsm_object_factory_asts_from_data (simulation_code, introspection_xml, &error);

	g_free (introspection_xml);
	g_free (simulation_code);

	if (error != NULL) {
		g_printerr (_("Error creating DFSM ASTs: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);
		g_free (command_line);

		exit (STATUS_INVALID_CODE);
	}

	/* Build the GraphViz code. */
	graphviz_string = g_string_sized_new (100); /* arbitrary */

	/* Introductory comment. */
	date_time = g_date_time_new_now_utc ();
	time_str = g_date_time_format (date_time, "%F %TZ" /* ISO 8601 date and time */);
	g_date_time_unref (date_time);

	if (object_path != NULL) {
		g_string_append_printf (graphviz_string,
		                        _("/* Graph of object with path ‘%s’ from simulation code file ‘%s’ and introspection XML ‘%s’. Generated on %s "
		                          "using command:\n"
		                          " * %s */"), object_path, simulation_filename, introspection_filename, time_str, command_line);
	} else {
		g_string_append_printf (graphviz_string,
		                        _("/* Graph of all objects from simulation code file ‘%s’ and introspection XML ‘%s’. Generated on %s using "
		                          "command:\n"
		                          " * %s */"), simulation_filename, introspection_filename, time_str, command_line);
	}

	g_string_append_c (graphviz_string, '\n');

	g_free (time_str);
	g_free (command_line);

	/* Start building the graph. */
	g_string_append_printf (graphviz_string, "digraph \"%s\" {\n", graph_id);

	for (i = 0; i < ast_objects->len; i++) {
		DfsmAstObject *ast_object;

		ast_object = DFSM_AST_OBJECT (g_ptr_array_index (ast_objects, i));

		/* If we only want to print a specific object, filter others out. */
		if (object_path != NULL && strcmp (object_path, dfsm_ast_object_get_object_path (ast_object)) != 0) {
			continue;
		}

		ast_object_print_graph (ast_object, graphviz_string);
		g_string_append_c (graphviz_string, '\n');

		if (object_path != NULL) {
			/* We can bail early now that we've found what we're looking for. */
			object_found = TRUE;
			break;
		}
	}

	g_string_append (graphviz_string, "}\n");

	g_ptr_array_unref (ast_objects);

	/* If we were looking for a specific object, did we find it? */
	if (object_path != NULL && object_found == FALSE) {
		g_printerr (_("Couldn’t find object with path ‘%s’ in simulation code."), object_path);
		g_printerr ("\n");

		g_string_free (graphviz_string, TRUE);

		exit (STATUS_OBJECT_NOT_FOUND);
	}

	/* Print it. */
	if (fputs (g_string_free (graphviz_string, FALSE), stdout) == EOF) {
		return STATUS_IO_ERROR;
	}

	return STATUS_SUCCESS;
}
