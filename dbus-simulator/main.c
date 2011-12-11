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

#include "dbus-daemon.h"
#include "logging.h"
#include "test-program.h"

enum StatusCodes {
	STATUS_SUCCESS = 0,
	STATUS_INVALID_OPTIONS = 1,
	STATUS_UNREADABLE_FILE = 2,
	STATUS_INVALID_CODE = 3,
	STATUS_DBUS_ERROR = 4,
	STATUS_DAEMON_SPAWN_ERROR = 5,
	STATUS_TEST_PROGRAM_SPAWN_ERROR = 6,
	STATUS_LOGGING_PROBLEM = 7,
};

static gint random_seed = 0;
static gchar *test_program_log_file = NULL;
static gint test_program_log_fd = 0;
static gchar *dbus_daemon_log_file = NULL;
static gint dbus_daemon_log_fd = 0;
static gchar *simulator_log_file = NULL;
static gint simulator_log_fd = 0;

static const GOptionEntry entries[] = {
	{ "random-seed", 's', 0, G_OPTION_ARG_INT, &random_seed, N_("Seed value for the simulation’s random number generator"), N_("SEED") },
	{ "test-program-log-file", 0, 0, G_OPTION_ARG_FILENAME, &test_program_log_file, N_("URI or path of a file to log test program output to"),
	  N_("FILE") },
	{ "test-program-log-fd", 0, 0, G_OPTION_ARG_INT, &test_program_log_fd, N_("Open FD to log test program output to"), N_("FD") },
	{ "dbus-daemon-log-file", 0, 0, G_OPTION_ARG_FILENAME, &dbus_daemon_log_file, N_("URI or path of a file to log dbus-daemon output to"),
	  N_("FILE") },
	{ "dbus-daemon-log-fd", 0, 0, G_OPTION_ARG_INT, &dbus_daemon_log_fd, N_("Open FD to log dbus-daemon output to"), N_("FD") },
	{ "simulator-log-file", 0, 0, G_OPTION_ARG_FILENAME, &simulator_log_file, N_("URI or path of a file to log simulator output to"),
	  N_("FILE") },
	{ "simulator-log-fd", 0, 0, G_OPTION_ARG_INT, &simulator_log_fd, N_("Open FD to log simulator output to"), N_("FD") },
	{ NULL }
};

static void
print_help_text (GOptionContext *context)
{
	gchar *help_text;

	help_text = g_option_context_get_help (context, TRUE, NULL);
	puts (help_text);
	g_free (help_text);
}

typedef struct {
	/* Program structure */
	GMainLoop *main_loop;
	int exit_status;

	/* Simulation gubbins */
	DsimTestProgram *test_program;
	gchar *test_program_name;
	GPtrArray/*<string>*/ *test_program_argv;
	DsimDBusDaemon *dbus_daemon;
	gchar *dbus_address;
	GDBusConnection *connection;
	guint outstanding_bus_ownership_callbacks; /* number of calls to g_bus_own_name() which are outstanding */
	GHashTable/*<string, uint>*/ *bus_name_ids; /* map from well-known bus name to its ownership ID */
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
} MainData;

static void
main_data_clear (MainData *data)
{
	g_clear_object (&data->connection);
	g_free (data->dbus_address);
	g_ptr_array_unref (data->simulated_objects);
	g_hash_table_unref (data->bus_name_ids);

	if (data->test_program != NULL) {
		dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->test_program));
		g_clear_object (&data->test_program);
	}

	g_ptr_array_unref (data->test_program_argv);
	g_free (data->test_program_name);

	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->dbus_daemon));
	g_clear_object (&data->dbus_daemon);

	g_main_loop_unref (data->main_loop);
}

static void
connection_close_cb (GObject *source_object, GAsyncResult *result, MainData *data)
{
	GError *error = NULL;

	/* Finish closing the connection */
	g_dbus_connection_close_finish (data->connection, result, &error);

	if (error != NULL) {
		g_printerr (_("Error closing D-Bus connection: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);
	}

	/* Kill the dbus-daemon instance. */
	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->dbus_daemon));

	/* Quit everything */
	g_main_loop_quit (data->main_loop);
}

static gboolean
simulation_timeout_cb (MainData *data)
{
	guint i, bus_name_id;
	GHashTableIter iter;

	/* Simulation's finished, so kill the test program. */
	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->test_program));

	/* Unregister all our DfsmObjects. */
	for (i = 0; i < data->simulated_objects->len; i++) {
		DfsmObject *simulated_object = g_ptr_array_index (data->simulated_objects, i);
		dfsm_object_unregister_on_bus (simulated_object);
	}

	/* Drop our well-known names. */
	g_hash_table_iter_init (&iter, data->bus_name_ids);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &bus_name_id) == TRUE) {
		g_bus_unown_name (bus_name_id);
	}

	/* Disconnect from the bus. */
	g_dbus_connection_close (data->connection, NULL, (GAsyncReadyCallback) connection_close_cb, data);

	return FALSE;
}

static void
spawn_test_program (MainData *data)
{
	GError *error = NULL;

	/* Spawn the program under test. */
	dsim_program_wrapper_spawn (DSIM_PROGRAM_WRAPPER (data->test_program), &error);

	if (error != NULL) {
		g_printerr (_("Error spawning test program instance: %s"), error->message);
		g_printerr ("\n");

		data->exit_status = STATUS_TEST_PROGRAM_SPAWN_ERROR;

		goto error;
	}

	/* TODO: How do we know when the simulation's finished? */
	g_timeout_add_seconds (60, (GSourceFunc) simulation_timeout_cb, data);

	return;

error:
	g_error_free (error);

	/* Convenient way to free everything. */
	simulation_timeout_cb (data);
}

static void
name_acquired_cb (GDBusConnection *connection, const gchar *name, MainData *data)
{
	g_debug ("Acquired ownership of well-known bus name: %s", name);

	/* Bail if this isn't the last callback. */
	data->outstanding_bus_ownership_callbacks--;
	if (data->outstanding_bus_ownership_callbacks > 0) {
		return;
	}

	/* Spawn! */
	spawn_test_program (data);
}

static void
name_lost_cb (GDBusConnection *connection, const gchar *name, MainData *data)
{
	g_debug ("Lost ownership of well-known bus name: %s", name);
}

static void
connection_created_cb (GObject *source_object, GAsyncResult *result, MainData *data)
{
	guint i;
	GError *error = NULL;

	/* Finish connecting to D-Bus. */
	data->connection = g_dbus_connection_new_finish (result, &error);

	if (error != NULL) {
		g_printerr (_("Error connecting to D-Bus using address ‘%s’: %s"), data->dbus_address, error->message);
		g_printerr ("\n");

		g_error_free (error);

		data->exit_status = STATUS_DBUS_ERROR;
		g_main_loop_quit (data->main_loop);
		return;
	}

	/* Register our DfsmObjects on the bus. */
	for (i = 0; i < data->simulated_objects->len; i++) {
		DfsmObject *simulated_object;

		simulated_object = g_ptr_array_index (data->simulated_objects, i);

		dfsm_object_register_on_bus (simulated_object, data->connection, &error);

		if (error != NULL) {
			/* Error! Unregister all the objects we've just registered, disconnect from the bus and run away. */
			g_printerr (_("Error connecting simulated object to D-Bus: %s"), error->message);
			g_printerr ("\n");

			data->exit_status = STATUS_DBUS_ERROR;

			goto error;
		}
	}

	/* Register our bus connection under zero or more well-known names. Once this is done, spawn the program under test. */
	for (i = 0; i < data->simulated_objects->len; i++) {
		DfsmObject *simulated_object;
		GPtrArray/*<string>*/ *bus_names;
		guint j;

		simulated_object = g_ptr_array_index (data->simulated_objects, i);
		bus_names = dfsm_object_get_well_known_bus_names (simulated_object);

		/* Hold an outstanding callback while we loop over the bus names, so that don't spawn the program under test before we've finished
		 * requesting to own all our bus names (e.g. if their callbacks are called very quickly. */
		data->outstanding_bus_ownership_callbacks++;

		for (j = 0; j < bus_names->len; j++) {
			const gchar *bus_name;
			guint bus_name_id;

			bus_name = g_ptr_array_index (bus_names, j);

			/* Skip the name if another object's already requested to own it. */
			if (g_hash_table_lookup_extended (data->bus_name_ids, bus_name, NULL, NULL) == TRUE) {
				continue;
			}

			/* Own the name. We keep a count of all the outstanding callbacks and only spawn the program under test once all are
			 * complete. */
			data->outstanding_bus_ownership_callbacks++;
			bus_name_id = g_bus_own_name_on_connection (data->connection, bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
			                                            (GBusNameAcquiredCallback) name_acquired_cb, (GBusNameLostCallback) name_lost_cb,
			                                            data, NULL);
			g_hash_table_insert (data->bus_name_ids, g_strdup (bus_name), GUINT_TO_POINTER (bus_name_id));
		}

		/* Release our outstanding callback and spawn the test program if it hasn't been spawned already. */
		data->outstanding_bus_ownership_callbacks--;
		if (data->outstanding_bus_ownership_callbacks == 0) {
			spawn_test_program (data);
		}
	}

	return;

error:
	g_error_free (error);

	/* Unregister objects */
	while (i-- > 0) {
		DfsmObject *simulated_object = g_ptr_array_index (data->simulated_objects, i);
		dfsm_object_unregister_on_bus (simulated_object);
	}

	/* Run away from the bus */
	g_dbus_connection_close (data->connection, NULL, (GAsyncReadyCallback) connection_close_cb, data);
}

static void
forward_envp_pair (GPtrArray/*<string>*/ *envp, const gchar *key)
{
	const gchar *value;

	value = g_getenv (key);
	if (value != NULL) {
		g_ptr_array_add (envp, g_strdup_printf ("%s=%s", key, value));
	}
}

static void
dbus_daemon_notify_bus_address_cb (GObject *gobject, GParamSpec *pspec, MainData *data)
{
	GPtrArray/*<string>*/ *test_program_envp;
	gchar *envp_pair;

	g_assert (data->dbus_address == NULL);
	data->dbus_address = g_strdup (dsim_dbus_daemon_get_bus_address (data->dbus_daemon));

	/* Set up the test program ready for spawning. */
	test_program_envp = g_ptr_array_new_with_free_func (g_free);

	envp_pair = g_strdup_printf ("DBUS_SESSION_BUS_ADDRESS=%s", data->dbus_address);
	g_ptr_array_add (test_program_envp, envp_pair);

	forward_envp_pair (test_program_envp, "DISPLAY");
	forward_envp_pair (test_program_envp, "XDG_DATA_HOME");
	forward_envp_pair (test_program_envp, "XDG_CACHE_HOME");
	forward_envp_pair (test_program_envp, "XDG_CONFIG_HOME");
	forward_envp_pair (test_program_envp, "HOME");
	forward_envp_pair (test_program_envp, "USER");
	forward_envp_pair (test_program_envp, "HOSTNAME");
	forward_envp_pair (test_program_envp, "SSH_CLIENT");
	forward_envp_pair (test_program_envp, "SSH_TTY");
	forward_envp_pair (test_program_envp, "SSH_CONNECTION");

	data->test_program = dsim_test_program_new (g_file_new_for_path ("/tmp/test"), data->test_program_name, data->test_program_argv,
	                                            test_program_envp);

	g_ptr_array_unref (test_program_envp);

	/* Start building a D-Bus connection with our new bus address. */
	g_dbus_connection_new_for_address (data->dbus_address,
	                                   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL,
	                                   (GAsyncReadyCallback) connection_created_cb, data);

	/* We don't want this to fire again. */
	g_signal_handlers_disconnect_by_func (gobject, dbus_daemon_notify_bus_address_cb, data);
}

int
main (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	const gchar *simulation_filename, *introspection_filename;
	gchar *simulation_code, *introspection_xml;
	MainData data;
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
	const gchar *test_program_name;
	GPtrArray/*<string>*/ *test_program_argv;
	guint i;

	/* Set up localisation. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();
	g_set_application_name (_("D-Bus Simulator"));

	/* Parse command line options */
	context = g_option_context_new (_("[simulation code file] [introspection XML file] -- [executable-file] [arguments]"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, _("Simulates the server in a D-Bus client–server conversation."));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

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

	/* Extract the remaining arguments */
	if (argc < 4) {
		g_printerr (_("Error parsing command line options: %s"), _("Test program must be provided"));
		g_printerr ("\n");

		print_help_text (context);

		exit (STATUS_INVALID_OPTIONS);
	}

	test_program_name = argv[3];
	test_program_argv = g_ptr_array_new_with_free_func (g_free);

	for (i = 4; i < (guint) argc; i++) {
		g_ptr_array_add (test_program_argv, g_strdup (argv[i]));
	}

	g_option_context_free (context);

	/* Set up logging. */
	dsim_logging_init (test_program_log_file, test_program_log_fd, dbus_daemon_log_file, dbus_daemon_log_fd, simulator_log_file, simulator_log_fd,
	                   &error);

	if (error != NULL) {
		g_printerr (_("Error setting up logging: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);

		exit (STATUS_LOGGING_PROBLEM);
	}

	/* Set up the random number generator. */
	if (random_seed == 0) {
		random_seed = (guint32) g_get_real_time ();
	}

	g_message (_("Note: Setting random number generator seed to %u."), random_seed);

	g_random_set_seed ((guint32) random_seed);

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

	/* Build the DfsmObjects. */
	simulated_objects = dfsm_object_factory_from_files (simulation_code, introspection_xml, &error);

	g_free (introspection_xml);
	g_free (simulation_code);

	if (error != NULL) {
		g_printerr (_("Error creating simulated DFSMs: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);

		exit (STATUS_INVALID_CODE);
	}

	/* Prepare the main data struct, which will last for the lifetime of the program. */
	data.main_loop = g_main_loop_new (NULL, FALSE);
	data.exit_status = STATUS_SUCCESS;
	data.simulated_objects = g_ptr_array_ref (simulated_objects);
	data.bus_name_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	data.outstanding_bus_ownership_callbacks = 0;

	g_ptr_array_unref (simulated_objects);

	/* Store the test program name and argv, since we can only spawn it once we know the bus address. */
	data.test_program_name = g_strdup (test_program_name);
	data.test_program_argv = g_ptr_array_ref (test_program_argv);

	g_ptr_array_unref (test_program_argv);

	/* Start up our own private dbus-daemon instance. */
	/* TODO */
	data.dbus_daemon = dsim_dbus_daemon_new (g_file_new_for_path ("/tmp/dbus"), g_file_new_for_path ("/tmp/dbus/config.xml"));
	data.dbus_address = NULL;
	g_signal_connect (data.dbus_daemon, "notify::bus-address", (GCallback) dbus_daemon_notify_bus_address_cb, &data);

	dsim_program_wrapper_spawn (DSIM_PROGRAM_WRAPPER (data.dbus_daemon), &error);

	if (error != NULL) {
		g_printerr (_("Error spawning private dbus-daemon instance: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);
		main_data_clear (&data);
		dsim_logging_finalise ();

		exit (STATUS_DAEMON_SPAWN_ERROR);
	}

	/* Start the main loop and wait for the dbus-daemon to send us its address. */
	g_main_loop_run (data.main_loop);

	/* Free the main data struct. */
	main_data_clear (&data);
	dsim_logging_finalise ();

	return data.exit_status;
}
