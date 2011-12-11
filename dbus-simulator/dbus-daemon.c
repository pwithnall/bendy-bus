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
#include "logging.h"

static void dsim_dbus_daemon_dispose (GObject *object);
static void dsim_dbus_daemon_finalize (GObject *object);
static void dsim_dbus_daemon_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dsim_dbus_daemon_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void dsim_dbus_daemon_build_argv (DsimProgramWrapper *wrapper, GPtrArray *argv);
static gboolean dsim_dbus_daemon_spawn_begin (DsimProgramWrapper *wrapper, GError **error);
static void dsim_dbus_daemon_spawn_end (DsimProgramWrapper *wrapper, GPid child_pid);
static void dsim_dbus_daemon_process_died (DsimProgramWrapper *wrapper, gint status);

struct _DsimDBusDaemonPrivate {
	GFile *configuration_file;

	/* Useful things */
	gchar *bus_address; /* initially NULL */

	/* Internal things */
	guint address_watch_id;
	gint address_pipe[2];
};

enum {
	PROP_CONFIGURATION_FILE = 1,
	PROP_BUS_ADDRESS,
};

G_DEFINE_TYPE (DsimDBusDaemon, dsim_dbus_daemon, DSIM_TYPE_PROGRAM_WRAPPER)

static void
dsim_dbus_daemon_class_init (DsimDBusDaemonClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DsimProgramWrapperClass *wrapper_class = DSIM_PROGRAM_WRAPPER_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DsimDBusDaemonPrivate));

	gobject_class->get_property = dsim_dbus_daemon_get_property;
	gobject_class->set_property = dsim_dbus_daemon_set_property;
	gobject_class->dispose = dsim_dbus_daemon_dispose;
	gobject_class->finalize = dsim_dbus_daemon_finalize;

	wrapper_class->build_argv = dsim_dbus_daemon_build_argv;
	wrapper_class->build_envp = NULL;
	wrapper_class->spawn_begin = dsim_dbus_daemon_spawn_begin;
	wrapper_class->spawn_end = dsim_dbus_daemon_spawn_end;
	wrapper_class->process_died = dsim_dbus_daemon_process_died;

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
	 * The address of the bus instance created by dbus-daemon. This will initially be %NULL, but will be set shortly after
	 * dsim_program_wrapper_spawn() has been called successfully. It will be an address as
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
}

static void
dsim_dbus_daemon_dispose (GObject *object)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (object)->priv;

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

static void
dsim_dbus_daemon_build_argv (DsimProgramWrapper *wrapper, GPtrArray *argv)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (wrapper)->priv;
	gchar *configuration_file_path;

	/* Static parameters. */
	g_ptr_array_add (argv, g_strdup ("--nofork"));

	/* Add the config file and address pipe FD to the argv */
	configuration_file_path = g_file_get_path (DSIM_DBUS_DAEMON (wrapper)->priv->configuration_file);
	g_ptr_array_add (argv, g_strdup_printf ("--config-file=%s", configuration_file_path));

	g_assert (priv->address_pipe[1] != -1);
	g_ptr_array_add (argv, g_strdup_printf ("--print-address=%i", priv->address_pipe[1]));
}

static gboolean
dsim_dbus_daemon_spawn_begin (DsimProgramWrapper *wrapper, GError **error)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (wrapper)->priv;

	/* Create a pipe for the daemon to write its address to */
	if (pipe (priv->address_pipe) != 0) {
		/* Error! */
		const gchar *message = g_strerror (errno);
		g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, _("Preparing pipes for dbus-daemon failed: %s"), message);

		return TRUE;
	}

	g_debug ("Opened address pipe from %i to %i.", priv->address_pipe[0], priv->address_pipe[1]);

	return FALSE;
}

static gboolean
address_channel_cb (GIOChannel *channel, GIOCondition condition, DsimDBusDaemon *self)
{
	DsimDBusDaemonPrivate *priv = self->priv;

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
		priv->address_pipe[0] = -1;

		return FALSE;
	}

	return TRUE;
}

static void
dsim_dbus_daemon_spawn_end (DsimProgramWrapper *wrapper, GPid child_pid)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (wrapper)->priv;
	GIOChannel *address_channel;

	if (child_pid == 0) {
		/* Failure. Close the pipes and tidy up. */
		close (priv->address_pipe[0]); priv->address_pipe[0] = -1;
		close (priv->address_pipe[1]); priv->address_pipe[1] = -1;

		return;
	}

	/* Listen on the address pipe, so that we can receive the daemon's address. We take this opportunity to close the other half of the pipe
	 * in the parent process, since we don't care about it. */
	address_channel = g_io_channel_unix_new (priv->address_pipe[0]);
	close (priv->address_pipe[1]); priv->address_pipe[1] = -1;

	/* Interpretation of http://dbus.freedesktop.org/doc/dbus-specification.html#addresses seems to imply the address is pure ASCII, so let's
	 * treat it as a byte stream so we don't attempt to re-encode it. */
	g_io_channel_set_encoding (address_channel, NULL, NULL);
	g_io_channel_set_buffered (address_channel, FALSE);

	priv->address_watch_id = g_io_add_watch (address_channel, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
	                                         (GIOFunc) address_channel_cb, wrapper);

	g_debug ("Listening to address pipe with watch ID %i.", priv->address_watch_id);

	g_io_channel_unref (address_channel);

	/* Success: store away relevant data. */
	priv->bus_address = NULL;
}

static void
dsim_dbus_daemon_process_died (DsimProgramWrapper *wrapper, gint status)
{
	DsimDBusDaemonPrivate *priv = DSIM_DBUS_DAEMON (wrapper)->priv;

	/* Daemon's died or been killed, so tidy everything up. */
	g_source_remove (priv->address_watch_id); priv->address_watch_id = 0;

	if (priv->bus_address != NULL) {
		g_free (priv->bus_address);
		priv->bus_address = NULL;
		g_object_notify (G_OBJECT (wrapper), "bus-address");
	}
}

/**
 * dsim_dbus_daemon_new:
 * @working_directory: directory to start the dbus-daemon in
 * @configuration_file: configuration file to pass to the dbus-daemon
 *
 * Creates a new #DsimDBusDaemon, but does not spawn any dbus-daemon processes yet. dsim_program_wrapper_spawn() does that.
 *
 * Return value: (transfer full): a new #DsimDBusDaemon
 */
DsimDBusDaemon *
dsim_dbus_daemon_new (GFile *working_directory, GFile *configuration_file)
{
	g_return_val_if_fail (G_IS_FILE (working_directory), NULL);
	g_return_val_if_fail (G_IS_FILE (configuration_file), NULL);

	return g_object_new (DSIM_TYPE_DBUS_DAEMON,
	                     "program-name", "dbus-daemon",
	                     "working-directory", working_directory,
	                     "configuration-file", configuration_file,
	                     "logging-domain-name", dsim_logging_get_domain_name (DSIM_LOG_DBUS_DAEMON),
	                     NULL);
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
