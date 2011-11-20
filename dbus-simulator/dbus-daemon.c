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

#include "dbus-daemon.h"

static void dsim_dbus_daemon_dispose (GObject *object);
static void dsim_dbus_daemon_finalize (GObject *object);
static void dsim_dbus_daemon_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dsim_dbus_daemon_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

struct _DsimDBusDaemonPrivate {
	GFile *working_directory;
	GFile *configuration_file;

	/* Useful things */
	GPid pid;
	gint stdout_fd;
	gint stderr_fd;
	guint pid_watch_id;
	gchar *bus_address; /* initially NULL */

	/* Internal things */
	guint address_watch_id;
	guint stdout_watch_id;
	guint stderr_watch_id;
};

enum {
	PROP_WORKING_DIRECTORY = 1,
	PROP_CONFIGURATION_FILE,
	PROP_BUS_ADDRESS,
};

G_DEFINE_TYPE (DsimDBusDaemon, dsim_dbus_daemon, G_TYPE_OBJECT)

static void
dsim_dbus_daemon_class_init (DsimDBusDaemonClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DsimDBusDaemonPrivate));

	gobject_class->get_property = dsim_dbus_daemon_get_property;
	gobject_class->set_property = dsim_dbus_daemon_set_property;
	gobject_class->dispose = dsim_dbus_daemon_dispose;
	gobject_class->finalize = dsim_dbus_daemon_finalize;

	/**
	 * DsimDBusDaemon:working-directory:
	 *
	 * Directory to start the dbus-daemon instance in.
	 */
	g_object_class_install_property (gobject_class, PROP_WORKING_DIRECTORY,
	                                 g_param_spec_object ("working-directory",
	                                                      "Working directory", "Directory to start the dbus-daemon instance in.",
	                                                      G_TYPE_FILE,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimDBusDaemon:configuration-file:
	 *
	 * Configuration file to pass to the dbus-daemon instance. This will be passed as its <code class="literal">--config-file</code> option.
	 *
	 * This should be a standard D-Bus configuration file as described by dbus-daemon(1).
	 */
	g_object_class_install_property (gobject_class, PROP_CONFIGURATION_FILE,
	                                 g_param_spec_object ("configuration-file",
	                                                      "Configuration file", "Configuration file to pass to the dbus-daemon instance.",
	                                                      G_TYPE_FILE,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimDBusDaemon:bus-address:
	 *
	 * The address of the bus instance created by dbus-daemon. This will initially be %NULL, but will be set shortly after dsim_dbus_daemon_spawn()
	 * has been called successfully. It will be an address as
	 * <ulink type="http" url="http://dbus.freedesktop.org/doc/dbus-specification.html#addresses">described in the D-Bus specification</ulink>.
	 */
	g_object_class_install_property (gobject_class, PROP_BUS_ADDRESS,
	                                 g_param_spec_string ("bus-address",
	                                                      "Bus address", "The address of the bus instance created by dbus-daemon.",
	                                                      NULL,
	                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
dsim_dbus_daemon_init (DsimDBusDaemon *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DSIM_TYPE_DBUS_DAEMON, DsimDBusDaemonPrivate);

	/* Set various FDs to default values. */
	self->priv->stderr_fd = -1;
	self->priv->stdout_fd = -1;
	self->priv->pid = -1;
}

static void
dsim_dbus_daemon_dispose (GObject *object)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (object)->priv;

	/* Ensure we kill the process first. */
	dsim_dbus_daemon_kill (DSIM_DBUS_DAEMON (object));

	g_clear_object (&priv->working_directory);
	g_clear_object (&priv->configuration_file);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dsim_dbus_daemon_parent_class)->dispose (object);
}

static void
dsim_dbus_daemon_finalize (GObject *object)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (object)->priv;

	g_free (priv->bus_address);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dsim_dbus_daemon_parent_class)->dispose (object);
}

static void
dsim_dbus_daemon_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (object)->priv;

	switch (property_id) {
		case PROP_WORKING_DIRECTORY:
			g_value_set_object (value, priv->working_directory);
			break;
		case PROP_CONFIGURATION_FILE:
			g_value_set_object (value, priv->configuration_file);
			break;
		case PROP_BUS_ADDRESS:
			g_value_set_object (value, priv->bus_address);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dsim_dbus_daemon_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (object)->priv;

	switch (property_id) {
		case PROP_WORKING_DIRECTORY:
			/* Construct-only */
			priv->working_directory = g_value_dup_object (value);
			break;
		case PROP_CONFIGURATION_FILE:
			/* Construct-only */
			priv->configuration_file = g_value_dup_object (value);
			break;
		case PROP_BUS_ADDRESS:
			/* Read-only */
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

/**
 * dsim_dbus_daemon_new:
 * @working_directory: directory to start the dbus-daemon in
 * @configuration_file: configuration file to pass to the dbus-daemon
 *
 * Creates a new #DsimDBusDaemon, but does not spawn any dbus-daemon processes yet. dsim_dbus_daemon_spawn() does that.
 *
 * Return value: (transfer full): a new #DsimDBusDaemon
 */
DsimDBusDaemon *
dsim_dbus_daemon_new (GFile *working_directory, GFile *configuration_file)
{
	g_return_val_if_fail (G_IS_FILE (working_directory), NULL);
	g_return_val_if_fail (G_IS_FILE (configuration_file), NULL);

	return g_object_new (DSIM_TYPE_DBUS_DAEMON,
	                     "working-directory", working_directory,
	                     "configuration-file", configuration_file,
	                     NULL);
}

static void
child_watch_cb (GPid pid, gint status, DsimDBusDaemon *self)
{
	DsimDBusDaemonPrivate *priv = self->priv;

	g_debug ("dbus-daemon died.");

	/* Daemon's died, so tidy everything up. */
	g_source_remove (priv->stderr_watch_id); priv->stderr_watch_id = 0;
	g_source_remove (priv->stdout_watch_id); priv->stdout_watch_id = 0;
	g_source_remove (priv->address_watch_id); priv->address_watch_id = 0;
	g_source_remove (priv->pid_watch_id); priv->pid_watch_id = 0;

	if (priv->bus_address != NULL) {
		g_free (priv->bus_address);
		priv->bus_address = NULL;
		g_object_notify (G_OBJECT (self), "bus-address");
	}

	close (priv->stderr_fd); priv->stderr_fd = -1;
	close (priv->stdout_fd); priv->stdout_fd = -1;

	g_spawn_close_pid (priv->pid); priv->pid = -1;
}

static gboolean
address_channel_cb (GIOChannel *channel, GIOCondition condition, DsimDBusDaemon *self)
{
	g_debug ("Received notification %u on address channel.", condition);

	if (condition & (G_IO_IN | G_IO_PRI)) {
		gchar *bus_address, *new_bus_address;

		bus_address = g_strdup ("");

		while (TRUE) {
			gchar buf[128];
			gsize buf_filled_length;
			GIOStatus status;
			GError *child_error = NULL;

			/* Read a buffer-load in */
			do {
				status = g_io_channel_read_chars (channel, buf, G_N_ELEMENTS (buf) - 1 /* nul */, &buf_filled_length, &child_error);
			} while (status == G_IO_STATUS_AGAIN);

			switch (status) {
				case G_IO_STATUS_NORMAL:
					/* Read this buffer-load successfully. After handling it, we'll loop around and read another. */

					/* Append what we have to the bus address, stripping any trailing newlines in the process. */
					if (buf[buf_filled_length - 1] == '\n') {
						buf[buf_filled_length - 1] = '\0';
					}
					buf[buf_filled_length] = '\0';

					new_bus_address = g_strconcat (bus_address, buf, NULL);
					g_free (bus_address);
					bus_address = new_bus_address;

					break;
				case G_IO_STATUS_EOF:
					/* We're finished, for now. Save the bus address and return. */
					g_debug ("Successfully read bus address from dbus-daemon: %s", bus_address);

					g_assert (self->priv->bus_address == NULL);
					self->priv->bus_address = bus_address;
					g_object_notify (G_OBJECT (self), "bus-address");

					goto in_done;
				case G_IO_STATUS_ERROR:
					/* Error! */
					g_warning ("Error reading address from dbus-daemon: %s", child_error->message);
					g_error_free (child_error);

					goto in_done;
				case G_IO_STATUS_AGAIN:
				default:
					g_assert_not_reached ();
			}
		}
	}

in_done:
	if (condition & (G_IO_ERR | G_IO_NVAL)) {
		/* Error. Close the FD and run away. */
		g_warning ("Error reading address from dbus-daemon: %s", "Error polling FD.");
		return FALSE;
	}

	if (condition & G_IO_HUP) {
		/* Other end's hung up, so we can stop listening and close the FD. */
		return FALSE;
	}

	return TRUE;
}

static gboolean
stdouterr_channel_cb (GIOChannel *channel, GIOCondition condition, const gchar *channel_name)
{
	g_debug ("Received notification %u on %s channel.", condition, channel_name);

	if (condition & (G_IO_IN | G_IO_PRI)) {
		while (TRUE) {
			gchar *stdouterr_line;
			GIOStatus status;
			GError *child_error = NULL;

			/* Read a line in */
			do {
				status = g_io_channel_read_line (channel, &stdouterr_line, NULL, NULL, &child_error);
			} while (status == G_IO_STATUS_AGAIN);

			switch (status) {
				case G_IO_STATUS_NORMAL:
					/* Read this line successfully. After handling it, we'll loop around and read another. */
					g_debug ("dbus-daemon %s: %s", channel_name, stdouterr_line);
					break; /* and continue the loop */
				case G_IO_STATUS_EOF:
					/* We're done! */
					goto in_done;
				case G_IO_STATUS_ERROR:
					/* Error! */
					g_warning ("Error reading %s line from dbus-daemon: %s", channel_name, child_error->message);
					g_error_free (child_error);

					goto in_done;
				case G_IO_STATUS_AGAIN:
				default:
					g_assert_not_reached ();
			}
		}
	}

in_done:
	if (condition & (G_IO_ERR | G_IO_NVAL)) {
		/* Error. Close the FD and run away. */
		g_warning ("Error reading %s from dbus-daemon: %s", channel_name, "Error polling FD.");
		return FALSE;
	}

	if (condition & G_IO_HUP) {
		/* Other end's hung up, so we can stop listening and close the FD. */
		return FALSE;
	}

	return TRUE;
}

/**
 * dsim_dbus_daemon_spawn:
 * @self: a #DsimDBusDaemon
 * @error: (allow-none): a #GError, or %NULL
 *
 * Spawns a new dbus-daemon instance to be controlled by this #DsimDBusDaemon. The process will be started asynchronously, so this function will
 * return without blocking. Consequently, any errors which cause the dbus-daemon process to quit with an error message after the fork-and-exec has
 * completed will not be reported by @error.
 *
 * If the dbus-daemon process has already been successfully spawned, this will return immediately without spawning it again.
 */
void
dsim_dbus_daemon_spawn (DsimDBusDaemon *self, GError **error)
{
	DsimDBusDaemonPrivate *priv;
	GPid child_pid;
	gint child_stdout, child_stderr;
	int address_pipe[2];
	GIOChannel *address_channel, *child_stdout_channel, *child_stderr_channel;
	guint address_watch_id, child_stdout_watch_id, child_stderr_watch_id;
	const gchar *locale_charset;
	GError *child_error = NULL;
	guint child_watch_id;
	gchar *command_line, *working_directory, *configuration_file;
	DsimDBusDaemon *daemon_data;

	const gchar *argv[] = { "dbus-daemon", "--nofork", NULL /* --config-file, set below */, NULL /* --print-address, set below */, NULL };
	const gchar *envp[] = { NULL };

	g_return_if_fail (DSIM_IS_DBUS_DAEMON (self));
	g_return_if_fail (error == NULL || *error == NULL);

	priv = self->priv;

	/* Is the process already running? */
	if (priv->pid != -1) {
		return;
	}

	/* Create a pipe for the daemon to write its address to */
	if (pipe (address_pipe) != 0) {
		/* Error! */
		const gchar *message = g_strerror (errno);
		g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, _("Preparing pipes for dbus-daemon failed: %s"), message);

		return;
	}

	/* Add the config file and address pipe FD to the argv */
	configuration_file = g_file_get_path (self->priv->configuration_file);

	argv[2] = g_strdup_printf ("--config-file=%s", configuration_file);
	argv[3] = g_strdup_printf ("--print-address=%i", address_pipe[1]);

	g_free (configuration_file);

	g_debug ("Opened address pipe from %i to %i.", address_pipe[0], address_pipe[1]);

	command_line = g_strjoinv (" ", (gchar**) argv);
	g_debug ("Spawning dbus-daemon with command line: %s", command_line);
	g_free (command_line);

	/* Spawn the dbus-daemon and don't allow it to daemonise. */
	working_directory = g_file_get_path (self->priv->working_directory);

	g_spawn_async_with_pipes (working_directory, (gchar**) argv, (gchar**) envp,
	                          G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
	                          NULL, NULL, &child_pid, NULL, &child_stdout, &child_stderr, &child_error);

	g_free (working_directory);

	g_free ((gchar*) argv[2]);
	g_free ((gchar*) argv[3]);

	if (child_error != NULL) {
		/* Error! */
		g_propagate_error (error, child_error);

		return;
	}

	g_debug ("Successfully spawned dbus-daemon as process %i, with stdout as %i and stderr as %i.", child_pid, child_stdout, child_stderr);

	/* Data struct */
	daemon_data = g_slice_new (DsimDBusDaemon);

	/* Listen on the address pipe, so that we can receive the daemon's address. We take this opportunity to close the other half of the pipe
	 * in the parent process, since we don't care about it. */
	address_channel = g_io_channel_unix_new (address_pipe[0]);
	close (address_pipe[1]);

	/* Interpretation of http://dbus.freedesktop.org/doc/dbus-specification.html#addresses seems to imply the address is pure ASCII, so let's
	 * treat it as a byte stream so we don't attempt to re-encode it. */
	g_io_channel_set_encoding (address_channel, NULL, NULL);
	g_io_channel_set_buffered (address_channel, FALSE);

	address_watch_id = g_io_add_watch (address_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                   (GIOFunc) address_channel_cb, daemon_data);

	g_debug ("Listening to address pipe with watch ID %i.", address_watch_id);

	g_io_channel_unref (address_channel);

	/* Listen for things on the daemon's stderr and stdout */
	g_get_charset (&locale_charset);

	child_stdout_channel = g_io_channel_unix_new (child_stdout);
	g_io_channel_set_encoding (child_stdout_channel, locale_charset, NULL);

	child_stdout_watch_id = g_io_add_watch (child_stdout_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                        (GIOFunc) stdouterr_channel_cb, (gpointer) "stdout");

	g_debug ("Listening to stdout pipe with watch ID %i.", child_stdout_watch_id);

	g_io_channel_unref (child_stdout_channel);

	child_stderr_channel = g_io_channel_unix_new (child_stderr);
	g_io_channel_set_encoding (child_stderr_channel, locale_charset, NULL);

	child_stderr_watch_id = g_io_add_watch (child_stderr_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                        (GIOFunc) stdouterr_channel_cb, (gpointer) "stderr");

	g_debug ("Listening to stderr pipe with watch ID %i.", child_stderr_watch_id);

	g_io_channel_unref (child_stderr_channel);

	/* Watch to see if the daemon exits */
	child_watch_id = g_child_watch_add (child_pid, (GChildWatchFunc) child_watch_cb, daemon_data);

	g_debug ("Watching child process with watch ID %u.", child_watch_id);

	/* Return a new structure containing all the relevant data. */
	priv->pid = child_pid;
	priv->stdout_fd = child_stdout;
	priv->stderr_fd = child_stderr;
	priv->pid_watch_id = child_watch_id;
	priv->bus_address = NULL;
	priv->address_watch_id = address_watch_id;
	priv->stdout_watch_id = child_stdout_watch_id;
	priv->stderr_watch_id = child_stderr_watch_id;
}

/**
 * dsim_dbus_daemon_kill:
 * @data: a #DsimDBusDaemon
 *
 * Kills the dbus-daemon process associated with this #DsimDBusDaemon which was previously started using dsim_dbus_daemon_spawn(). This will result
 * in #DsimDBusDaemon:bus-address being reset to %NULL.
 *
 * If the process is no longer running (either because dsim_dbus_daemon_spawn() was never called successfully, because the process has died of its
 * own accord, or because dsim_dbus_daemon_kill() has previously been called successfully), this function will return immediately.
 */
void
dsim_dbus_daemon_kill (DsimDBusDaemon *self)
{
	g_return_if_fail (DSIM_IS_DBUS_DAEMON (self));

	/* Already killed? */
	if (self->priv->pid == -1) {
		g_debug ("Skipping killing dbus-daemon (already dead).");
		return;
	}

	/* Send a SIGTERM to the dbus-daemon process. */
	g_debug ("Killing dbus-daemon.");

	kill (self->priv->pid, SIGTERM);
}

/**
 * dsim_dbus_daemon_get_bus_address:
 * @data: a #DsimDBusDaemon
 *
 * Gets the value of #DsimDBusDaemon:bus-address.
 *
 * Return value: the address of the dbus-daemon process associated with this #DsimDBusDaemon (if it's running), or %NULL
 */
const gchar *
dsim_dbus_daemon_get_bus_address (DsimDBusDaemon *self)
{
	g_return_val_if_fail (DSIM_IS_DBUS_DAEMON (self), NULL);

	return self->priv->bus_address;
}
