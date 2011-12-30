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

#include "logging.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixoutputstream.h>

typedef struct {
	gchar **debug_domains;
	struct {
		GOutputStream *log_stream;
		guint log_id;
	} domains[DSIM_NUM_LOGGING_DOMAINS];
} DsimLogs;

static DsimLogs dsim_logs;

static GOutputStream *
open_log_file_or_fd (const gchar *log_filename, gint log_fd, GError **error)
{
	GOutputStream *output_stream;
	GError *child_error = NULL;

	g_return_val_if_fail (log_filename == NULL || log_fd == 0, NULL);

	if (log_filename != NULL) {
		GFile *log_file;

		/* Log file given by absolute or relative path, or URI. */
		log_file = g_file_new_for_commandline_arg (log_filename);
		output_stream = G_OUTPUT_STREAM (g_file_append_to (log_file, G_FILE_CREATE_NONE, NULL, &child_error));
		g_object_unref (log_file);

		if (child_error != NULL) {
			g_propagate_error (error, child_error);
			return NULL;
		}
	} else {
		/* Log stream given by an open FD. */
		output_stream = g_unix_output_stream_new (log_fd, FALSE);
	}

	return output_stream;
}

static void
log_handler_cb (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer domain_id_pointer)
{
	DsimLoggingDomain domain;
	gchar *formatted_message;
	const gchar *log_level_string = "";
	GError *child_error = NULL;

	domain = GPOINTER_TO_UINT (domain_id_pointer);

	/* Bail if it's a debug message and we aren't displaying debug messages from this domain. */
	if (log_level & G_LOG_LEVEL_DEBUG) {
		gchar **debug_domain;
		gboolean found = FALSE;

		for (debug_domain = dsim_logs.debug_domains; *debug_domain != NULL; debug_domain++) {
			if (strcmp (*debug_domain, log_domain) == 0) {
				found = TRUE;
				break;
			}
		}

		if (found == FALSE) {
			return;
		}
	}

	/* Handle recursion. */
	if (log_level & G_LOG_FLAG_RECURSION) {
		/* This can only happen in case of an error, below. Consequently, we do things the old fashioned way. */
		puts (message);
		return;
	}

	/* Format the message. */
	if (log_level & G_LOG_LEVEL_DEBUG) {
		log_level_string = " DEBUG:";
	} else if (log_level & G_LOG_LEVEL_WARNING) {
		log_level_string = " WARNING:";
	} else if (log_level & G_LOG_LEVEL_CRITICAL) {
		log_level_string = " CRITICAL:";
	} else if (log_level & G_LOG_LEVEL_ERROR) {
		log_level_string = " ERROR:";
	}

	formatted_message = g_strdup_printf ("%" G_GINT64_FORMAT ":%s %s\n", g_get_monotonic_time (), log_level_string, message);

	/* Write the message to the specified output stream. */
	if (g_output_stream_write_all (dsim_logs.domains[domain].log_stream, formatted_message, strlen (formatted_message), NULL, NULL,
	                               &child_error) == FALSE) {
		/* Error writing to the log. */
		g_log (dsim_logging_get_domain_name (DSIM_LOG_SIMULATOR), G_LOG_LEVEL_WARNING | G_LOG_FLAG_RECURSION,
		       _("Error writing to log for domain ‘%s’: %s"), log_domain, child_error->message);
		g_error_free (child_error);

		goto done;
	}

done:
	g_free (formatted_message);
}

void
dsim_logging_init (const gchar *test_program_log_filename, gint test_program_log_fd,
                   const gchar *dbus_daemon_log_filename, gint dbus_daemon_log_fd,
                   const gchar *simulator_log_filename, gint simulator_log_fd, GError **error)
{
	guint i;
	const gchar *messages_debug;
	GError *child_error = NULL;

	g_return_if_fail (test_program_log_filename == NULL || test_program_log_fd == 0);
	g_return_if_fail (dbus_daemon_log_filename == NULL || dbus_daemon_log_fd == 0);
	g_return_if_fail (simulator_log_filename == NULL || simulator_log_fd == 0);

	/* Defaults. */
	if (test_program_log_filename == NULL && test_program_log_fd == 0) {
		test_program_log_fd = STDOUT_FILENO;
	}

	if (dbus_daemon_log_filename == NULL && dbus_daemon_log_fd == 0) {
		dbus_daemon_log_fd = 0; /* blackhole it */
	}

	if (simulator_log_filename == NULL && simulator_log_fd == 0) {
		simulator_log_fd = STDOUT_FILENO;
	}

	/* Open all the log streams. */
	dsim_logs.domains[DSIM_LOG_TEST_PROGRAM].log_stream = open_log_file_or_fd (test_program_log_filename, test_program_log_fd, &child_error);

	if (child_error != NULL) {
		goto error;
	}

	dsim_logs.domains[DSIM_LOG_DBUS_DAEMON].log_stream = open_log_file_or_fd (dbus_daemon_log_filename, dbus_daemon_log_fd, &child_error);

	if (child_error != NULL) {
		goto error;
	}

	dsim_logs.domains[DSIM_LOG_SIMULATOR].log_stream = open_log_file_or_fd (simulator_log_filename, simulator_log_fd, &child_error);

	if (child_error != NULL) {
		goto error;
	}

	/* The two simulator domains share an output. */
	dsim_logs.domains[DSIM_LOG_SIMULATOR_LIBRARY].log_stream = g_object_ref (dsim_logs.domains[DSIM_LOG_SIMULATOR].log_stream);

	/* Install log handlers. */
	for (i = 0; i < DSIM_NUM_LOGGING_DOMAINS; i++) {
		dsim_logs.domains[i].log_id = g_log_set_handler (dsim_logging_get_domain_name (i),
		                                                 G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
		                                                 (GLogFunc) log_handler_cb, GUINT_TO_POINTER (i));
	}

	/* Parse the G_MESSAGES_DEBUG environment variable to find the debug domains which are enabled. */
	messages_debug = g_getenv ("G_MESSAGES_DEBUG");

	if (messages_debug == NULL) {
		messages_debug = "";
	}

	dsim_logs.debug_domains = g_strsplit (messages_debug, " ", 0);

	return;

error:
	dsim_logging_finalise ();

	g_propagate_error (error, child_error);
}

void
dsim_logging_finalise (void)
{
	guint i;

	if (dsim_logs.debug_domains != NULL) {
		g_strfreev (dsim_logs.debug_domains);
		dsim_logs.debug_domains = NULL;
	}

	for (i = 0; i < DSIM_NUM_LOGGING_DOMAINS; i++) {
		g_clear_object (&dsim_logs.domains[i].log_stream);

		if (dsim_logs.domains[i].log_id != 0) {
			g_log_remove_handler (dsim_logging_get_domain_name (i), dsim_logs.domains[i].log_id);
			dsim_logs.domains[i].log_id = 0;
		}
	}
}

/* This must match DsimLoggingDomain from logging.h. */
static const gchar *logging_domain_names[] = {
	"test-program", /* DSIM_LOG_TEST_PROGRAM */
	"dbus-daemon", /* DSIM_LOG_DBUS_DAEMON */
	"bendy-bus", /* DSIM_LOG_SIMULATOR */
	"libdfsm", /* DSIM_LOG_SIMULATOR_LIBRARY */
};

const gchar *
dsim_logging_get_domain_name (DsimLoggingDomain domain_id)
{
	g_return_val_if_fail (G_N_ELEMENTS (logging_domain_names) == DSIM_NUM_LOGGING_DOMAINS, NULL);
	g_return_val_if_fail (/* domain_id >= 0 || */ domain_id < DSIM_NUM_LOGGING_DOMAINS, NULL);

	return logging_domain_names[domain_id];
}
