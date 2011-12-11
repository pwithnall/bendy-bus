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

#include <errno.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "program-wrapper.h"
#include "marshal.h"

static void dsim_program_wrapper_dispose (GObject *object);
static void dsim_program_wrapper_finalize (GObject *object);
static void dsim_program_wrapper_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dsim_program_wrapper_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _DsimProgramWrapperPrivate {
	GFile *working_directory;
	gchar *program_name;
	gchar *logging_domain_name;

	/* Useful things */
	GPid pid;
	gboolean process_is_running;
	gint stdout_fd;
	gint stderr_fd;
	guint pid_watch_id;

	/* Internal things */
	guint stdout_watch_id;
	guint stderr_watch_id;
};

enum {
	PROP_WORKING_DIRECTORY = 1,
	PROP_PROGRAM_NAME,
	PROP_PROCESS_ID,
	PROP_LOGGING_DOMAIN_NAME,
};

enum {
	SIGNAL_SPAWN_BEGIN,
	SIGNAL_SPAWN_END,
	SIGNAL_PROCESS_DIED,
	LAST_SIGNAL,
};

static guint program_wrapper_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_ABSTRACT_TYPE (DsimProgramWrapper, dsim_program_wrapper, G_TYPE_OBJECT)

static void
dsim_program_wrapper_class_init (DsimProgramWrapperClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DsimProgramWrapperPrivate));

	gobject_class->get_property = dsim_program_wrapper_get_property;
	gobject_class->set_property = dsim_program_wrapper_set_property;
	gobject_class->dispose = dsim_program_wrapper_dispose;
	gobject_class->finalize = dsim_program_wrapper_finalize;

	/**
	 * DsimProgramWrapper:working-directory:
	 *
	 * Directory to start the program instance in.
	 */
	g_object_class_install_property (gobject_class, PROP_WORKING_DIRECTORY,
	                                 g_param_spec_object ("working-directory",
	                                                      "Working directory", "Directory to start the program instance in.",
	                                                      G_TYPE_FILE,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimProgramWrapper:program-name:
	 *
	 * Name of the executable to spawn.
	 */
	g_object_class_install_property (gobject_class, PROP_PROGRAM_NAME,
	                                 g_param_spec_string ("program-name",
	                                                      "Program name", "Name of the executable to spawn.",
	                                                      NULL,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimProgramWrapper:process-id:
	 *
	 * Process ID of the wrapped program.
	 */
	g_object_class_install_property (gobject_class, PROP_PROCESS_ID,
	                                 g_param_spec_int ("process-id",
	                                                   "Process ID", "Process ID of the wrapped program.",
	                                                   -1, G_MAXINT, -1,
	                                                   G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimProgramWrapper:logging-domain-name:
	 *
	 * Name of the logging domain name to use for all stdout/stderr output from the process.
	 */
	g_object_class_install_property (gobject_class, PROP_LOGGING_DOMAIN_NAME,
	                                 g_param_spec_string ("logging-domain-name",
	                                                      "Logging domain name",
	                                                      "Name of the logging domain name to use for all stdout/stderr output from the process.",
	                                                      NULL,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimProgramWrapper::spawn-begin:
	 *
	 * Emitted just before spawning the process owned by this #DsimProgramWrapper. This is guaranteed to be emitted before the argv is built.
	 */
	program_wrapper_signals[SIGNAL_SPAWN_BEGIN] = g_signal_new ("spawn-begin",
	                                                            G_TYPE_FROM_CLASS (klass),
	                                                            G_SIGNAL_RUN_LAST,
	                                                            G_STRUCT_OFFSET (DsimProgramWrapperClass, spawn_begin),
	                                                            g_signal_accumulator_true_handled, NULL,
	                                                            dsim_marshal_BOOLEAN__POINTER,
	                                                            G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);

	/**
	 * DsimProgramWrapper::spawn-end:
	 *
	 * Emitted just after spawning the process owned by this #DsimProgramWrapper, regardless of whether spawning was successful. If successful,
	 * the process' PID will be passed to the signal handler. If unsuccessful, <code class="literal">0</code> will be passed instead.
	 */
	program_wrapper_signals[SIGNAL_SPAWN_END] = g_signal_new ("spawn-end",
	                                                          G_TYPE_FROM_CLASS (klass),
	                                                          G_SIGNAL_RUN_LAST,
	                                                          G_STRUCT_OFFSET (DsimProgramWrapperClass, spawn_end), NULL, NULL,
	                                                          g_cclosure_marshal_VOID__INT,
	                                                          G_TYPE_NONE, 1, G_TYPE_INT);

	/**
	 * DsimProgramWrapper::process-died:
	 * @status: the exit status of the process
	 *
	 * Emitted when the process owned by this #DsimProgramWrapper dies or exits of its own accord, or is killed by the wrapper.
	 */
	program_wrapper_signals[SIGNAL_PROCESS_DIED] = g_signal_new ("process-died",
	                                                             G_TYPE_FROM_CLASS (klass),
	                                                             G_SIGNAL_RUN_LAST,
	                                                             G_STRUCT_OFFSET (DsimProgramWrapperClass, process_died), NULL, NULL,
	                                                             g_cclosure_marshal_VOID__INT,
	                                                             G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
dsim_program_wrapper_init (DsimProgramWrapper *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DSIM_TYPE_PROGRAM_WRAPPER, DsimProgramWrapperPrivate);

	/* Set various FDs to default values. */
	self->priv->stderr_fd = -1;
	self->priv->stdout_fd = -1;
	self->priv->pid = -1;
	self->priv->process_is_running = FALSE;
}

static void
dsim_program_wrapper_dispose (GObject *object)
{
	DsimProgramWrapperPrivate *priv = DSIM_PROGRAM_WRAPPER (object)->priv;

	/* Ensure we kill the process first. */
	dsim_program_wrapper_kill (DSIM_PROGRAM_WRAPPER (object));

	g_clear_object (&priv->working_directory);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dsim_program_wrapper_parent_class)->dispose (object);
}

static void
dsim_program_wrapper_finalize (GObject *object)
{
	DsimProgramWrapperPrivate *priv = DSIM_PROGRAM_WRAPPER (object)->priv;

	g_free (priv->program_name);
	g_free (priv->logging_domain_name);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dsim_program_wrapper_parent_class)->dispose (object);
}

static void
dsim_program_wrapper_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DsimProgramWrapperPrivate *priv = DSIM_PROGRAM_WRAPPER (object)->priv;

	switch (property_id) {
		case PROP_WORKING_DIRECTORY:
			g_value_set_object (value, priv->working_directory);
			break;
		case PROP_PROGRAM_NAME:
			g_value_set_string (value, priv->program_name);
			break;
		case PROP_PROCESS_ID:
			g_value_set_int (value, priv->pid);
			break;
		case PROP_LOGGING_DOMAIN_NAME:
			g_value_set_string (value, priv->logging_domain_name);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dsim_program_wrapper_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DsimProgramWrapperPrivate *priv = DSIM_PROGRAM_WRAPPER (object)->priv;

	switch (property_id) {
		case PROP_WORKING_DIRECTORY:
			/* Construct-only */
			priv->working_directory = g_value_dup_object (value);
			break;
		case PROP_PROGRAM_NAME:
			/* Construct-only */
			priv->program_name = g_value_dup_string (value);
			break;
		case PROP_LOGGING_DOMAIN_NAME:
			/* Construct-only */
			priv->logging_domain_name = g_value_dup_string (value);
			break;
		case PROP_PROCESS_ID:
			/* Read-only */
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
child_watch_cb (GPid pid, gint status, DsimProgramWrapper *self)
{
	DsimProgramWrapperPrivate *priv = self->priv;

	g_debug ("`%s` died.", priv->program_name);

	/* Signal emission. */
	g_signal_emit (self, program_wrapper_signals[SIGNAL_PROCESS_DIED], 0, status);

	/* Daemon's died, so tidy everything up. */
	g_source_remove (priv->stderr_watch_id); priv->stderr_watch_id = 0;
	g_source_remove (priv->stdout_watch_id); priv->stdout_watch_id = 0;
	g_source_remove (priv->pid_watch_id); priv->pid_watch_id = 0;

	close (priv->stderr_fd); priv->stderr_fd = -1;
	close (priv->stdout_fd); priv->stdout_fd = -1;

	/* NOTE: We retain the PID for use by dsim_program_wrapper_get_process_id(). */
	g_spawn_close_pid (priv->pid);

	priv->process_is_running = FALSE;
}

static gboolean
stdouterr_channel_cb (GIOChannel *channel, GIOCondition condition, gpointer user_data)
{
	const gchar *channel_name;
	DsimProgramWrapper *self;

	/* Decode the channel name from the user_data pointer. */
	if (((gsize) user_data & 1) == 0) {
		channel_name = "stdout";
		self = DSIM_PROGRAM_WRAPPER ((gsize) user_data ^ 0);
	} else {
		channel_name = "stderr";
		self = DSIM_PROGRAM_WRAPPER ((gsize) user_data ^ 1);
	}

	g_debug ("Received notification %u on %s channel.", condition, channel_name);

	if (condition & (G_IO_IN | G_IO_PRI)) {
		while (TRUE) {
			gchar stdouterr_buf[256];
			gsize bytes_read = 0;
			GIOStatus status;
			GError *child_error = NULL;

			/* Read a buffer-load in */
			status = g_io_channel_read_chars (channel, stdouterr_buf, sizeof (stdouterr_buf) - 1 /* nul */, &bytes_read, &child_error);
			stdouterr_buf[bytes_read] = '\0'; /* nul terminator */

			switch (status) {
				case G_IO_STATUS_NORMAL:
					/* Read this line successfully. After handling it, we'll loop around and read another. */
					g_log (dsim_program_wrapper_get_logging_domain_name (self), G_LOG_LEVEL_MESSAGE, "%s: %s", channel_name,
					       stdouterr_buf);

					if (bytes_read != sizeof (stdouterr_buf)) {
						/* Done? */
						goto in_done;
					}

					break; /* and continue the loop */
				case G_IO_STATUS_EOF:
					/* We're done! */
					g_log (dsim_program_wrapper_get_logging_domain_name (self), G_LOG_LEVEL_MESSAGE, "%s: %s", channel_name,
					       stdouterr_buf);
					goto in_done;
				case G_IO_STATUS_ERROR:
					/* Error! */
					g_warning ("Error reading %s line from process: %s", channel_name, child_error->message);
					g_error_free (child_error);

					goto in_done;
				case G_IO_STATUS_AGAIN:
					/* Ignore and handle it next time. */
					goto in_done;
				default:
					g_assert_not_reached ();
			}
		}
	}

in_done:
	if (condition & (G_IO_ERR | G_IO_NVAL)) {
		/* Error. Close the FD and run away. */
		g_warning ("Error reading %s from process: %s", channel_name, "Error polling FD.");
		return FALSE;
	}

	if (condition & G_IO_HUP) {
		/* Other end's hung up, so we can stop listening and close the FD. */
		return FALSE;
	}

	return TRUE;
}

/**
 * dsim_program_wrapper_spawn:
 * @self: a #DsimProgramWrapper
 * @error: (allow-none): a #GError, or %NULL
 *
 * Spawns a new dbus-daemon instance to be controlled by this #DsimProgramWrapper. The process will be started asynchronously, so this function will
 * return without blocking. Consequently, any errors which cause the process to quit with an error message after the fork-and-exec has completed will
 * not be reported by @error.
 *
 * If the process has already been successfully spawned, this will return immediately without spawning it again.
 */
void
dsim_program_wrapper_spawn (DsimProgramWrapper *self, GError **error)
{
	DsimProgramWrapperPrivate *priv;
	DsimProgramWrapperClass *klass;
	GPid child_pid;
	gint child_stdout, child_stderr;
	GIOChannel *child_stdout_channel, *child_stderr_channel;
	guint child_stdout_watch_id, child_stderr_watch_id;
	const gchar *locale_charset;
	GError *child_error = NULL;
	guint child_watch_id;
	gchar *command_line, *environment, *working_directory;
	GPtrArray/*<string>*/ *argv, *envp;
	gboolean retval = FALSE;

	g_return_if_fail (DSIM_IS_PROGRAM_WRAPPER (self));
	g_return_if_fail (error == NULL || *error == NULL);

	priv = self->priv;
	klass = DSIM_PROGRAM_WRAPPER_GET_CLASS (self);

	/* Is the process already running? */
	if (priv->process_is_running == TRUE) {
		return;
	}

	/* Signal that we're about to start spawning. */
	g_signal_emit (self, program_wrapper_signals[SIGNAL_SPAWN_BEGIN], 0, &child_error, &retval);
	g_assert (retval == (child_error != NULL));

	if (child_error != NULL) {
		/* Error! */
		g_propagate_error (error, child_error);

		/* Signal failure. */
		g_signal_emit (self, program_wrapper_signals[SIGNAL_SPAWN_END], 0, 0);

		return;
	}

	/* Build command line and environment. */
	argv = g_ptr_array_new_with_free_func (g_free);
	envp = g_ptr_array_new_with_free_func (g_free);

	/* Program name. */
	g_ptr_array_add (argv, g_strdup (priv->program_name));

	if (klass->build_argv != NULL) {
		klass->build_argv (self, argv);
	}

	g_ptr_array_add (argv, NULL); /* NULL terminated */

	if (klass->build_envp != NULL) {
		klass->build_envp (self, envp);
	}

	g_ptr_array_add (envp, NULL); /* NULL terminated */

	command_line = g_strjoinv (" ", (gchar**) argv->pdata);
	environment = g_strjoinv (" ", (gchar**) envp->pdata);
	g_debug ("Spawning:\nCommand line: %s\nEnvironment: %s", command_line, environment);
	g_free (environment);
	g_free (command_line);

	/* Spawn the program. */
	working_directory = g_file_get_path (self->priv->working_directory);

	g_spawn_async_with_pipes (working_directory, (gchar**) argv->pdata, (gchar**) envp->pdata,
	                          G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
	                          NULL, NULL, &child_pid, NULL, &child_stdout, &child_stderr, &child_error);

	g_free (working_directory);

	g_ptr_array_unref (envp);
	g_ptr_array_unref (argv);

	if (child_error != NULL) {
		/* Error! */
		g_propagate_error (error, child_error);

		/* Signal failure. */
		g_signal_emit (self, program_wrapper_signals[SIGNAL_SPAWN_END], 0, 0);

		return;
	}

	g_debug ("Successfully spawned process %i, with stdout as %i and stderr as %i.", child_pid, child_stdout, child_stderr);

	priv->process_is_running = TRUE;

	/* Listen for things on the daemon's stderr and stdout. We hackily pass extra information in the user_data for the callbacks; we set the LSB
	 * of the pointer to be 1 iff the channel is stderr and 0 iff it's stdout. The rest of the bits contain the self pointer. */
	g_assert (((gsize) self & 1) == 0); /* will our hack work? */

	g_get_charset (&locale_charset);

	child_stdout_channel = g_io_channel_unix_new (child_stdout);
	g_io_channel_set_flags (child_stdout_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding (child_stdout_channel, locale_charset, NULL);

	child_stdout_watch_id = g_io_add_watch (child_stdout_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                        (GIOFunc) stdouterr_channel_cb, (gpointer) ((gsize) self ^ 0 /* stdout */));

	g_debug ("Listening to stdout pipe with watch ID %i.", child_stdout_watch_id);

	g_io_channel_unref (child_stdout_channel);

	child_stderr_channel = g_io_channel_unix_new (child_stderr);
	g_io_channel_set_flags (child_stderr_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding (child_stderr_channel, locale_charset, NULL);

	child_stderr_watch_id = g_io_add_watch (child_stderr_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                        (GIOFunc) stdouterr_channel_cb, (gpointer) ((gsize) self ^ 1 /* stderr */));

	g_debug ("Listening to stderr pipe with watch ID %i.", child_stderr_watch_id);

	g_io_channel_unref (child_stderr_channel);

	/* Watch to see if the daemon exits */
	child_watch_id = g_child_watch_add (child_pid, (GChildWatchFunc) child_watch_cb, self);

	g_debug ("Watching child process with watch ID %u.", child_watch_id);

	/* Success: store all the relevant data. */
	priv->pid = child_pid;
	priv->stdout_fd = child_stdout;
	priv->stderr_fd = child_stderr;
	priv->pid_watch_id = child_watch_id;
	priv->stdout_watch_id = child_stdout_watch_id;
	priv->stderr_watch_id = child_stderr_watch_id;

	/* Signal success. */
	g_signal_emit (self, program_wrapper_signals[SIGNAL_SPAWN_END], 0, child_pid);
}

/**
 * dsim_program_wrapper_kill:
 * @data: a #DsimProgramWrapper
 *
 * Kills the process associated with this #DsimProgramWrapper which was previously started using dsim_program_wrapper_spawn().
 *
 * If the process is no longer running (either because dsim_program_wrapper_spawn() was never called successfully, because the process has died of its
 * own accord, or because dsim_program_wrapper_kill() has previously been called successfully), this function will return immediately.
 */
void
dsim_program_wrapper_kill (DsimProgramWrapper *self)
{
	DsimProgramWrapperPrivate *priv;

	g_return_if_fail (DSIM_IS_PROGRAM_WRAPPER (self));

	priv = self->priv;

	/* Already killed? */
	if (self->priv->process_is_running == FALSE) {
		g_debug ("Skipping killing `%s` (already dead).", priv->program_name);
		return;
	}

	/* Send a SIGTERM to the dbus-daemon process. */
	g_debug ("Killing `%s`.", priv->program_name);

	kill (self->priv->pid, SIGTERM);
}

/**
 * dsim_program_wrapper_get_working_directory:
 * @self: a #DsimProgramWrapper
 *
 * Gets the value of the #DsimProgramWrapper:working-directory property. This property is independent of the lifecycle of the program being spawned or
 * killed, and will always be non-%NULL.
 *
 * Return value: (transfer none): the program's working directory
 */
GFile *
dsim_program_wrapper_get_working_directory (DsimProgramWrapper *self)
{
	g_return_val_if_fail (DSIM_IS_PROGRAM_WRAPPER (self), NULL);

	return self->priv->working_directory;
}

/**
 * dsim_program_wrapper_get_process_id:
 * @self: a #DsimProgramWrapper
 *
 * Gets the process ID of the wrapped program. This will be <code class="literal">-1</code> if the program hasn't yet been run using
 * dsim_program_wrapper_spawn(). After dsim_program_wrapper_spawn() has been called, the process ID will be set and will remain set even after the
 * wrapped program exits.
 *
 * Return value: process ID of the wrapped program, or <code class="literal">-1</code> if it hasn't been spawned yet
 */
GPid
dsim_program_wrapper_get_process_id (DsimProgramWrapper *self)
{
	g_return_val_if_fail (DSIM_IS_PROGRAM_WRAPPER (self), -1);

	return self->priv->pid;
}

/**
 * dsim_program_wrapper_get_logging_domain_name:
 * @self: a #DsimProgramWrapper
 *
 * Gets the string name of the logging domain which will be used for all stdout and stderr output from the process.
 *
 * Return value: name of the logging domain for the process
 */
const gchar *
dsim_program_wrapper_get_logging_domain_name (DsimProgramWrapper *self)
{
	g_return_val_if_fail (DSIM_IS_PROGRAM_WRAPPER (self), NULL);

	return self->priv->logging_domain_name;
}
