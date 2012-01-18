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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-unix.h>
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

static gint64 random_seed = 0;
static gchar *test_program_log_file = NULL;
static gint test_program_log_fd = 0;
static gchar *dbus_daemon_log_file = NULL;
static gint dbus_daemon_log_fd = 0;
static gchar *simulator_log_file = NULL;
static gint simulator_log_fd = 0;
static gint test_timeout = 0;
static gint run_time = 0;
static gint run_iters = 0;
static gboolean run_infinitely = FALSE;
static GPtrArray *test_program_environment = NULL;
static gboolean pass_through_environment = FALSE;

static gboolean
option_env_parse_cb (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
	guint equals_count = 0;
	const gchar *i;

	/* Parse the key-value pair. We expect something of the form ‘KEY=VALUE’ where both KEY and VALUE are non-empty and neither contain equals
	 * signs. */
	for (i = value; *i != '\0'; i++) {
		if (*i == '=') {
			equals_count++;
		}
	}

	if (equals_count != 1 || *value == '=' || *(i - 1) == '=') {
		g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, _("Invalid key-value pair (should be of the form: ‘KEY=VALUE’): %s"),
		             value);
		return FALSE;
	}

	/* Lazily create the array. */
	if (test_program_environment == NULL) {
		test_program_environment = g_ptr_array_new_with_free_func (g_free);
	}

	/* Set the pair. */
	g_ptr_array_add (test_program_environment, g_strdup (value));

	return TRUE;
}

static const GOptionEntry main_entries[] = {
	{ "random-seed", 's', 0, G_OPTION_ARG_INT64, &random_seed, N_("Seed value for the simulation’s random number generator"), N_("SEED") },
	{ NULL }
};

static const GOptionEntry logging_entries[] = {
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

static const GOptionEntry testing_entries[] = {
	{ "test-timeout", 't', 0, G_OPTION_ARG_INT, &test_timeout, N_("Timeout (in seconds) for a test run to be aborted if no D-Bus activity occurs"),
	  N_("SECS") },
	{ "run-time", 'r', 0, G_OPTION_ARG_INT, &run_time, N_("Maximum time (in seconds) the set of test runs should take"), N_("SECS") },
	{ "run-iters", 'n', 0, G_OPTION_ARG_INT, &run_iters, N_("Maximum number of test runs which should be performed (default: 1)"), N_("COUNT") },
	{ "run-infinitely", 'i', 0, G_OPTION_ARG_NONE, &run_infinitely, N_("Run test runs in an infinite loop"), NULL },
	{ NULL }
};

static const GOptionEntry test_program_entries[] = {
	{ "env", 'E', 0, G_OPTION_ARG_CALLBACK, option_env_parse_cb, N_("Define an environment key-value pair for the program under test"),
	  N_("KEY=VALUE") },
	{ "pass-through-environment", 0, 0, G_OPTION_ARG_NONE, &pass_through_environment,
	  N_("Pass through the environment from the simulator to the program under test"), NULL },
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
	int exit_signal;
	#define EXIT_SIGNAL_INVALID 0

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
	guint num_test_runs_remaining;
	guint test_run_inactivity_timeout_id;
} MainData;

static void
main_data_clear (MainData *data)
{
	g_clear_object (&data->connection);
	g_free (data->dbus_address);
	g_ptr_array_unref (data->simulated_objects);
	g_hash_table_unref (data->bus_name_ids);

	if (data->test_run_inactivity_timeout_id != 0) {
		g_source_remove (data->test_run_inactivity_timeout_id);
		data->test_run_inactivity_timeout_id = 0;
	}

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
post_connection_closed (MainData *data)
{
	/* Kill the dbus-daemon instance. */
	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->dbus_daemon));

	/* Quit everything */
	g_main_loop_quit (data->main_loop);
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

	post_connection_closed (data);
}

static void restart_simulation (MainData *data);

static gboolean
test_inactivity_timeout_cb (MainData *data)
{
	/* The current test run has hit a timeout; move on to the next test run. */
	restart_simulation (data);
	return FALSE;
}

static void
remove_inactivity_timeout (MainData *data)
{
	/* Remove our inactivity timeout. */
	if (data->test_run_inactivity_timeout_id != 0) {
		g_source_remove (data->test_run_inactivity_timeout_id);
		data->test_run_inactivity_timeout_id = 0;
	}
}

static void
set_inactivity_timeout (MainData *data)
{
	/* Set up the timeout. */
	if (test_timeout > 0) {
		data->test_run_inactivity_timeout_id = g_timeout_add_seconds (test_timeout, (GSourceFunc) test_inactivity_timeout_cb, data);
	}
}

static void
simulated_object_dbus_activity_count_notify_cb (GObject *obj, GParamSpec *pspec, MainData *data)
{
	remove_inactivity_timeout (data);
	set_inactivity_timeout (data);
}

static void
stop_simulation (MainData *data)
{
	guint i;

	g_debug ("stop_simulation()");

	/* Stop timers. */
	if (data->test_run_inactivity_timeout_id != 0) {
		g_source_remove (data->test_run_inactivity_timeout_id);
		data->test_run_inactivity_timeout_id = 0;
	}

	/* Simulation's finished, so kill the test program. */
	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->test_program));

	/* Drop our well-known names, but only if we haven't been signalled. If we have been signalled, the dbus-daemon has also been signalled,
	 * and so is concurrently dying. Since g_bus_unown_name() has no error handling, we can't prevent it spewing warnings everywhere about
	 * failing to interact with the dying dbus-daemon. */
	if (data->exit_signal == EXIT_SIGNAL_INVALID) {
		GHashTableIter iter;
		guint bus_name_id;

		g_hash_table_iter_init (&iter, data->bus_name_ids);

		while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &bus_name_id) == TRUE) {
			g_bus_unown_name (bus_name_id);
		}
	}

	g_hash_table_remove_all (data->bus_name_ids);

	/* Unregister all our DfsmObjects. */
	for (i = 0; i < data->simulated_objects->len; i++) {
		DfsmObject *simulated_object = g_ptr_array_index (data->simulated_objects, i);
		dfsm_object_unregister_on_bus (simulated_object);

		g_signal_handlers_disconnect_by_func (simulated_object, simulated_object_dbus_activity_count_notify_cb, data);
	}

	/* Disconnect from the bus. */
	if (data->connection != NULL) {
		g_dbus_connection_close (data->connection, NULL, (GAsyncReadyCallback) connection_close_cb, data);
	} else {
		/* Connection's already closed. */
		post_connection_closed (data);
	}
}

static void
spawn_test_program (MainData *data)
{
	GError *error = NULL;

	dsim_program_wrapper_spawn (DSIM_PROGRAM_WRAPPER (data->test_program), &error);

	if (data->num_test_runs_remaining > 0) {
		data->num_test_runs_remaining--;
	}

	if (error != NULL) {
		g_printerr (_("Error spawning test program instance: %s"), error->message);
		g_printerr ("\n");

		g_error_free (error);

		data->exit_status = STATUS_TEST_PROGRAM_SPAWN_ERROR;
		stop_simulation (data);

		return;
	}
}

static void
restart_simulation (MainData *data)
{
	guint i;

	/* Have we finished? */
	if (data->num_test_runs_remaining == 0) {
		g_message (_("Stopping simulation due to performing the desired number of test runs."));

		stop_simulation (data);
		return;
	}

	g_message (_("Restarting simulation."));

	/* Stop the test program and reset all our simulation objects. */
	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (data->test_program));

	for (i = 0; i < data->simulated_objects->len; i++) {
		DfsmObject *simulated_object = g_ptr_array_index (data->simulated_objects, i);
		dfsm_object_reset (simulated_object);
	}

	/* Re-spawn the program under test. */
	spawn_test_program (data);
}

static gboolean
restart_simulation_idle_cb (MainData *data)
{
	restart_simulation (data);
	return FALSE;
}

static void
test_program_died_cb (DsimProgramWrapper *wrapper, gint status, MainData *data)
{
	if (WIFEXITED (status) || (WIFSIGNALED (status) && (WTERMSIG (status) == SIGTERM || WTERMSIG (status) == SIGINT))) {
		/* Exited normally: proceed to the next test run. However, if bendy-bus was signalled beforehand, ignore the test program exiting
		 * and continue to close ourselves. */
		if (data->exit_signal == EXIT_SIGNAL_INVALID) {
			/* We have to do this in an idle callback so that we don't try to re-spawn the test program while it's still closing. */
			g_idle_add ((GSourceFunc) restart_simulation_idle_cb, data);
		}
	} else {
		/* Crashed: stop the entire simulation. */
		g_message (_("Stopping simulation due to test program crashing (status: %i)."), status);

		stop_simulation (data);
	}
}

static void
dbus_daemon_died_cb (DsimProgramWrapper *wrapper, gint status, MainData *data)
{
	/* This should never happen. We assume the dbus-daemon is rock solid. */
	if (WIFEXITED (status) || (WIFSIGNALED (status) && (WTERMSIG (status) == SIGTERM || WTERMSIG (status) == SIGINT))) {
		/* Ignore the daemon disappearing if bendy-bus was signalled beforehand. */
		if (data->exit_signal == EXIT_SIGNAL_INVALID) {
			g_message (_("Stopping simulation due to dbus-daemon exiting (status: %i)."), status);
		}
	} else {
		g_message (_("Stopping simulation due to dbus-daemon crashing (status: %i)."), status);
	}

	if (data->exit_signal != EXIT_SIGNAL_INVALID) {
		return;
	}

	/* Have we created/spawned the test program yet? If not, we don't have much cleaning up to do. */
	if (data->test_program != NULL) {
		stop_simulation (data);
	} else {
		g_main_loop_quit (data->main_loop);
	}
}

static gboolean
simulation_timeout_cb (MainData *data)
{
	g_message (_("Stopping simulation due to simulation timeout being reached."));
	stop_simulation (data);

	return FALSE;
}

static void
test_program_spawn_end_cb (DsimProgramWrapper *program_wrapper, GPid pid, MainData *data)
{
	if (run_time > 0) {
		g_timeout_add_seconds (run_time, (GSourceFunc) simulation_timeout_cb, data);
	}

	set_inactivity_timeout (data);
}

static void
start_simulation (MainData *data)
{
	g_message (_("Starting simulation."));

	g_signal_connect (data->test_program, "spawn-end", (GCallback) test_program_spawn_end_cb, data);
	g_signal_connect (data->test_program, "process-died", (GCallback) test_program_died_cb, data);

	/* Spawn the program under test. */
	spawn_test_program (data);

	/* The simulation's finished when either:
	 *  • We've run for at least run_time seconds (over all test runs).
	 *  • We've run at least run_iters number of iterations.
	 *  • The test program crashes.
	 *  • The dbus-daemon crashes or exits normally (this should never happen).
	 *
	 * A single test run is finished when either:
	 *  • We go without D-Bus activity for at least test_timeout seconds.
	 *  • The test program exits normally.
	 *
	 * All applicable timers start from after the (first) spawning of the test program is complete.
	 */
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
	start_simulation (data);
}

static void
name_lost_cb (GDBusConnection *connection, const gchar *name, MainData *data)
{
	g_debug ("Lost ownership of well-known bus name: %s", name);
}

static void
connection_closed_cb (GDBusConnection *connection, gboolean remote_peer_vanished, GError *error, MainData *data)
{
	g_debug ("D-Bus connection closed (remote peer vanished: %s, error: %s).", (remote_peer_vanished == TRUE) ? "yes" : "no",
	         (error != NULL) ? error->message : "no");

	/* Shut down if this wasn't the result of us calling close() on the connection. */
	if (remote_peer_vanished == TRUE || error != NULL) {
		g_clear_object (&data->connection);

		stop_simulation (data);
	}
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

	/* Connect to closed notifications, so we know if the bus disappears. */
	g_signal_connect (data->connection, "closed", (GCallback) connection_closed_cb, data);

	/* Register our DfsmObjects on the bus. */
	for (i = 0; i < data->simulated_objects->len; i++) {
		DfsmObject *simulated_object;

		simulated_object = g_ptr_array_index (data->simulated_objects, i);

		g_signal_connect (simulated_object, "notify::dbus-activity-count", (GCallback) simulated_object_dbus_activity_count_notify_cb, data);
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
			start_simulation (data);
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

	g_message (_("Note: Simulated bus has address: %s"), data->dbus_address);

	/* Set up the test program ready for spawning. */
	test_program_envp = g_ptr_array_new_with_free_func (g_free);

	if (pass_through_environment == FALSE) {
		/* Set up a minimal environment with just the necessary environment variables required for things to function.
		 * Note that this list of necessary environment variables is probably incomplete, and is built on the basis of
		 * trial and error rather than research or standards. */
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
	} else {
		/* Forward everything from the simulator's environment to the test program's environment. This might make the test
		 * results slightly less reliable. */
		gchar **env_variable_names;
		const gchar * const *i;

		env_variable_names = g_listenv ();

		for (i = (const gchar * const *) env_variable_names; *i != NULL; i++) {
			forward_envp_pair (test_program_envp, *i);
		}

		g_strfreev (env_variable_names);
	}

	envp_pair = g_strdup_printf ("DBUS_SESSION_BUS_ADDRESS=%s", data->dbus_address);
	g_ptr_array_add (test_program_envp, envp_pair);

	/* Copy the environment pairs set on the command line. */
	if (test_program_environment != NULL) {
		guint i;

		for (i = 0; i < test_program_environment->len; i++) {
			envp_pair = g_ptr_array_index (test_program_environment, i);
			g_ptr_array_add (test_program_envp, g_strdup (envp_pair));
		}
	}

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

static void
signal_handler (int signum, MainData *data)
{
	g_debug ("signal_handler(%i) called.", signum);

	/* We need to propagate the signal when we exit, in order that the calling process knows we exited due to a signal.
	 * However, if we kill() now, we won't finish cleaning up properly (because we need to re-enter the main loop for a little
	 * while to be able to do that). */
	data->exit_status = STATUS_SUCCESS;
	data->exit_signal = signum;

	/* Have we created/spawned the test program yet? If not, we don't have much cleaning up to do. */
	if (data->test_program != NULL) {
		g_message (_("Stopping simulation due to receiving termination signal."));
		stop_simulation (data);
	} else {
		g_main_loop_quit (data->main_loop);
	}
}

static gboolean
sigterm_handler_cb (MainData *data)
{
	signal_handler (SIGTERM, data);
	return FALSE;
}

static gboolean
sigint_handler_cb (MainData *data)
{
	signal_handler (SIGINT, data);
	return FALSE;
}

int
main (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionGroup *option_group;
	GOptionContext *context;
	const gchar *simulation_filename, *introspection_filename;
	gchar *simulation_code, *introspection_xml;
	MainData data;
	GPtrArray/*<DfsmObject>*/ *simulated_objects;
	const gchar *test_program_name;
	GPtrArray/*<string>*/ *test_program_argv;
	guint i;
	gchar *time_str, *command_line, *log_header, *seed_str;
	GDateTime *date_time;

	/* Set up localisation. */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_type_init ();
	g_set_application_name (_("D-Bus Simulator"));

	/* Take a copy of the command line, for use in printing the log headers later. */
	command_line = g_strjoinv (" ", argv);

	/* Parse command line options */
	context = g_option_context_new (_("[simulation code file] [introspection XML file] -- [executable-file] [arguments]"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_set_summary (context, _("Simulates the server in a D-Bus client–server conversation."));
	g_option_context_add_main_entries (context, main_entries, GETTEXT_PACKAGE);

	/* Logging option group */
	option_group = g_option_group_new ("logging", _("Logging Options:"), _("Show help options for output logging"), NULL, NULL);
	g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
	g_option_group_add_entries (option_group, logging_entries);
	g_option_context_add_group (context, option_group);

	/* Testing option group */
	option_group = g_option_group_new ("testing", _("Testing Options:"), _("Show help options for test runs and timeouts"), NULL, NULL);
	g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
	g_option_group_add_entries (option_group, testing_entries);
	g_option_context_add_group (context, option_group);

	/* Testing option group */
	option_group = g_option_group_new ("test-program", _("Test Program Options:"), _("Show help options for the program under test"), NULL, NULL);
	g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);
	g_option_group_add_entries (option_group, test_program_entries);
	g_option_context_add_group (context, option_group);

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

	/* Extract the remaining arguments */
	if (argc < 4) {
		g_printerr (_("Error parsing command line options: %s"), _("Test program must be provided"));
		g_printerr ("\n");

		print_help_text (context);

		g_option_context_free (context);
		g_free (command_line);

		exit (STATUS_INVALID_OPTIONS);
	}

	/* Work out where the test program's command line starts. g_option_context_parse() sometimes leaves the ‘--’ in argv. */
	if (strcmp (argv[3], "--") == 0) {
		i = 4;
	} else {
		i = 3;
	}

	test_program_name = argv[i++];
	test_program_argv = g_ptr_array_new_with_free_func (g_free);

	for (; i < (guint) argc; i++) {
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
		g_free (command_line);

		exit (STATUS_LOGGING_PROBLEM);
	}

	/* Output a log header to each of the log streams. */
	date_time = g_date_time_new_now_utc ();
	time_str = g_date_time_format (date_time, "%F %TZ");
	g_date_time_unref (date_time);

	log_header = g_strdup_printf (_("Bendy Bus (number %s) left the depot at %s using command line: %s"), PACKAGE_VERSION, time_str, command_line);

	g_log (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, "%s", log_header);
	g_log (dsim_logging_get_domain_name (DSIM_LOG_DBUS_DAEMON), G_LOG_LEVEL_MESSAGE, "%s", log_header);
	g_log (dsim_logging_get_domain_name (DSIM_LOG_TEST_PROGRAM), G_LOG_LEVEL_MESSAGE, "%s", log_header);

	g_free (log_header);
	g_free (time_str);
	g_free (command_line);

	/* Set up the random number generator. */
	if (random_seed == 0) {
		random_seed = g_get_real_time ();
	}

	seed_str = g_strdup_printf ("%" G_GINT64_FORMAT, random_seed);
	g_message (_("Note: Setting random number generator seed to %s."), seed_str);
	g_free (seed_str);

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
	data.exit_signal = EXIT_SIGNAL_INVALID;
	data.test_program = NULL;
	data.connection = NULL;
	data.simulated_objects = g_ptr_array_ref (simulated_objects);
	data.bus_name_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	data.outstanding_bus_ownership_callbacks = 0;
	data.test_run_inactivity_timeout_id = 0;

	if (run_infinitely == TRUE || (run_iters == 0 && run_time == 0)) {
		data.num_test_runs_remaining = -1;
	} else {
		data.num_test_runs_remaining = run_iters;
	}

	g_ptr_array_unref (simulated_objects);

	/* Store the test program name and argv, since we can only spawn it once we know the bus address. */
	data.test_program_name = g_strdup (test_program_name);
	data.test_program_argv = g_ptr_array_ref (test_program_argv);

	g_ptr_array_unref (test_program_argv);

	/* Set up signal handlers for SIGINT and SIGTERM so that we can close gracefully. */
	g_unix_signal_add (SIGINT, (GSourceFunc) sigint_handler_cb, &data);
	g_unix_signal_add (SIGTERM, (GSourceFunc) sigterm_handler_cb, &data);

	/* Start up our own private dbus-daemon instance. */
	/* TODO */
	data.dbus_daemon = dsim_dbus_daemon_new (g_file_new_for_path ("/tmp/dbus"), g_file_new_for_path ("/tmp/dbus/config.xml"));
	data.dbus_address = NULL;

	g_signal_connect (data.dbus_daemon, "process-died", (GCallback) dbus_daemon_died_cb, &data);
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

	if (data.exit_signal != EXIT_SIGNAL_INVALID) {
		struct sigaction action;

		/* Propagate the signal to the default handler. */
		action.sa_handler = SIG_DFL;
		sigemptyset (&action.sa_mask);
		action.sa_flags = 0;

		sigaction (data.exit_signal, &action, NULL);

		kill (getpid (), data.exit_signal);
	}

	return data.exit_status;
}
