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

/**
 * SECTION:dfsm-dbus-output-sequence
 * @short_description: D-Bus output sequence
 * @stability: Unstable
 * @include: dfsm/dfsm-dbus-output-sequence.h
 *
 * D-Bus based implementation of #DfsmOutputSequence which allows for replies to method calls (successful or erroneous ones) and emits signals onto
 * the bus. All actions are queued up when added to the output sequence, and are only propagated to the bus when dfsm_output_sequence_output() is
 * called.
 */

#include <string.h>
#include <glib.h>

#include "dfsm-dbus-output-sequence.h"
#include "dfsm-output-sequence.h"

typedef enum {
	ENTRY_REPLY,
	ENTRY_THROW,
	ENTRY_EMIT,
} QueueEntryType;

typedef struct {
	QueueEntryType entry_type;
	union {
		struct {
			GVariant *parameters;
		} reply;
		struct {
			GError *error;
		} throw;
		struct {
			gchar *interface_name;
			gchar *signal_name;
			GVariant *parameters;
		} emit;
	};
} QueueEntry;

static void
queue_entry_free (QueueEntry *entry)
{
	switch (entry->entry_type) {
		case ENTRY_REPLY:
			g_variant_unref (entry->reply.parameters);
			break;
		case ENTRY_THROW:
			g_error_free (entry->throw.error);
			break;
		case ENTRY_EMIT:
			g_free (entry->emit.interface_name);
			g_free (entry->emit.signal_name);
			g_variant_unref (entry->emit.parameters);
			break;
		default:
			g_assert_not_reached ();
	}

	g_slice_free (QueueEntry, entry);
}

static void dfsm_dbus_output_sequence_iface_init (DfsmOutputSequenceInterface *iface);
static void dfsm_dbus_output_sequence_dispose (GObject *object);
static void dfsm_dbus_output_sequence_finalize (GObject *object);
static void dfsm_dbus_output_sequence_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dfsm_dbus_output_sequence_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void dfsm_dbus_output_sequence_output (DfsmOutputSequence *sequence, GError **error);
static void dfsm_dbus_output_sequence_add_reply (DfsmOutputSequence *sequence, GVariant *parameters);
static void dfsm_dbus_output_sequence_add_throw (DfsmOutputSequence *sequence, GError *throw_error);
static void dfsm_dbus_output_sequence_add_emit (DfsmOutputSequence *sequence, const gchar *interface_name, const gchar *signal_name,
                                                GVariant *parameters);

struct _DfsmDBusOutputSequencePrivate {
	GDBusConnection *connection;
	gchar *object_path;
	GDBusMethodInvocation *invocation;
	GQueue/*<QueueEntry>*/ output_queue; /* head is the oldest entry (i.e. the one to get executed first) */
};

enum {
	PROP_CONNECTION = 1,
	PROP_OBJECT_PATH,
	PROP_METHOD_INVOCATION,
};

G_DEFINE_TYPE_EXTENDED (DfsmDBusOutputSequence, dfsm_dbus_output_sequence, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (DFSM_TYPE_OUTPUT_SEQUENCE, dfsm_dbus_output_sequence_iface_init))

static void
dfsm_dbus_output_sequence_class_init (DfsmDBusOutputSequenceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DfsmDBusOutputSequencePrivate));

	gobject_class->get_property = dfsm_dbus_output_sequence_get_property;
	gobject_class->set_property = dfsm_dbus_output_sequence_set_property;
	gobject_class->dispose = dfsm_dbus_output_sequence_dispose;
	gobject_class->finalize = dfsm_dbus_output_sequence_finalize;

	g_object_class_install_property (gobject_class, PROP_CONNECTION,
	                                 g_param_spec_object ("connection",
	                                                      "Connection", "D-Bus connection.",
	                                                      G_TYPE_DBUS_CONNECTION,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_OBJECT_PATH,
	                                 g_param_spec_string ("object-path",
	                                                      "Object Path", "Path of the D-Bus object this output sequence is for.",
	                                                      NULL,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_METHOD_INVOCATION,
	                                 g_param_spec_object ("method-invocation",
	                                                      "Method Invocation",
	                                                      "Data about the D-Bus method invocation which triggered this output sequence.",
	                                                      G_TYPE_DBUS_METHOD_INVOCATION,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
dfsm_dbus_output_sequence_init (DfsmDBusOutputSequence *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DFSM_TYPE_DBUS_OUTPUT_SEQUENCE, DfsmDBusOutputSequencePrivate);

	/* Initialise the queue. */
	g_queue_init (&self->priv->output_queue);
}

static void
dfsm_dbus_output_sequence_iface_init (DfsmOutputSequenceInterface *iface)
{
	iface->output = dfsm_dbus_output_sequence_output;
	iface->add_reply = dfsm_dbus_output_sequence_add_reply;
	iface->add_throw = dfsm_dbus_output_sequence_add_throw;
	iface->add_emit = dfsm_dbus_output_sequence_add_emit;
}

static void
dfsm_dbus_output_sequence_dispose (GObject *object)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (object)->priv;

	g_clear_object (&priv->invocation);
	g_clear_object (&priv->connection);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_dbus_output_sequence_parent_class)->dispose (object);
}

static void
dfsm_dbus_output_sequence_finalize (GObject *object)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (object)->priv;
	QueueEntry *queue_entry;

	/* Free any remaining entries in the queue. */
	while ((queue_entry = g_queue_pop_head (&priv->output_queue)) != NULL) {
		queue_entry_free (queue_entry);
	}

	g_free (priv->object_path);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dfsm_dbus_output_sequence_parent_class)->finalize (object);
}

static void
dfsm_dbus_output_sequence_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (object)->priv;

	switch (property_id) {
		case PROP_CONNECTION:
			g_value_set_object (value, priv->connection);
			break;
		case PROP_OBJECT_PATH:
			g_value_set_string (value, priv->object_path);
			break;
		case PROP_METHOD_INVOCATION:
			g_value_set_object (value, priv->invocation);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dfsm_dbus_output_sequence_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (object)->priv;

	switch (property_id) {
		case PROP_CONNECTION:
			/* Construct-only */
			priv->connection = g_value_dup_object (value);
			break;
		case PROP_OBJECT_PATH:
			/* Construct-only */
			priv->object_path = g_value_dup_string (value);
			break;
		case PROP_METHOD_INVOCATION:
			/* Construct-only */
			priv->invocation = g_value_dup_object (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dfsm_dbus_output_sequence_output (DfsmOutputSequence *sequence, GError **error)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;
	GError *child_error = NULL;

	/* Loop through the queue. */
	while (child_error == NULL && (queue_entry = g_queue_pop_head (&priv->output_queue)) != NULL) {
		switch (queue_entry->entry_type) {
			case ENTRY_REPLY: {
				gchar *reply_parameters_string;

				/* Reply to the method call. */
				g_assert (priv->invocation != NULL);
				g_dbus_method_invocation_return_value (priv->invocation, queue_entry->reply.parameters);

				/* Debug output. */
				reply_parameters_string = g_variant_print (queue_entry->reply.parameters, FALSE);
				g_debug ("Replying to D-Bus method call with out parameters: %s", reply_parameters_string);
				g_free (reply_parameters_string);

				break;
			}
			case ENTRY_THROW: {
				/* Reply to the method call with an error. */
				g_dbus_method_invocation_return_gerror (priv->invocation, queue_entry->throw.error);

				/* Debug output. */
				g_debug ("Throwing D-Bus error with domain ‘%s’ and code %i. Message: %s",
				         g_quark_to_string (queue_entry->throw.error->domain), queue_entry->throw.error->code,
				         queue_entry->throw.error->message);

				break;
			}
			case ENTRY_EMIT: {
				gchar *emit_parameters_string;

				/* Emit a signal. */
				g_dbus_connection_emit_signal (priv->connection, NULL, priv->object_path, queue_entry->emit.interface_name,
				                               queue_entry->emit.signal_name, queue_entry->emit.parameters, &child_error);

				/* Debug output. */
				emit_parameters_string = g_variant_print (queue_entry->emit.parameters, FALSE);
				g_debug ("Emitting D-Bus signal ‘%s’ on interface ‘%s’ of object ‘%s’. Parameters: %s", queue_entry->emit.signal_name,
				         queue_entry->emit.interface_name, priv->object_path, emit_parameters_string);
				g_free (emit_parameters_string);

				/* Error? Skip the rest of the output. The remaining entries will be cleaned up when the OutputSequence is finalised.
				 * Note that we're only supposed to encounter errors here if the signal name is invalid (and similar such situations),
				 * so it's debatable that this code will ever be called. */
				if (child_error != NULL) {
					g_propagate_error (error, child_error);
					break;
				}

				break;
			}
			default:
				g_assert_not_reached ();
		}

		queue_entry_free (queue_entry);
	}
}

static void
dfsm_dbus_output_sequence_add_reply (DfsmOutputSequence *sequence, GVariant *parameters)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;

	queue_entry = g_slice_new (QueueEntry);
	queue_entry->entry_type = ENTRY_REPLY;
	queue_entry->reply.parameters = g_variant_ref (parameters);

	g_queue_push_tail (&priv->output_queue, queue_entry);
}

static void
dfsm_dbus_output_sequence_add_throw (DfsmOutputSequence *sequence, GError *throw_error)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;

	queue_entry = g_slice_new (QueueEntry);
	queue_entry->entry_type = ENTRY_THROW;
	queue_entry->throw.error = g_error_copy (throw_error);

	g_queue_push_tail (&priv->output_queue, queue_entry);
}

static void
dfsm_dbus_output_sequence_add_emit (DfsmOutputSequence *sequence, const gchar *interface_name, const gchar *signal_name, GVariant *parameters)
{
	DfsmDBusOutputSequencePrivate *priv = DFSM_DBUS_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;

	queue_entry = g_slice_new (QueueEntry);
	queue_entry->entry_type = ENTRY_EMIT;
	queue_entry->emit.interface_name = g_strdup (interface_name);
	queue_entry->emit.signal_name = g_strdup (signal_name);
	queue_entry->emit.parameters = g_variant_ref (parameters);

	g_queue_push_tail (&priv->output_queue, queue_entry);
}

/**
 * dfsm_dbus_output_sequence_new:
 * @connection: a D-Bus connection to output the sequence over
 * @object_path: D-Bus path of the object the output sequence will occur on
 * @invocation: (allow-none): details of the triggering method invocation, or %NULL
 *
 * Create a new #DfsmDBusOutputSequence.
 *
 * Return value: (transfer full): a new #DfsmDBusOutputSequence
 */
DfsmDBusOutputSequence *
dfsm_dbus_output_sequence_new (GDBusConnection *connection, const gchar *object_path, GDBusMethodInvocation *invocation)
{
	g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
	g_return_val_if_fail (object_path != NULL && *object_path != '\0', NULL);
	g_return_val_if_fail (invocation == NULL || G_IS_DBUS_METHOD_INVOCATION (invocation), NULL);
	g_return_val_if_fail (invocation == NULL || g_dbus_method_invocation_get_connection (invocation) == connection, NULL);
	g_return_val_if_fail (invocation == NULL || strcmp (object_path, g_dbus_method_invocation_get_object_path (invocation)) == 0, NULL);

	return g_object_new (DFSM_TYPE_DBUS_OUTPUT_SEQUENCE,
	                     "connection", connection,
	                     "object-path", object_path,
	                     "method-invocation", invocation,
	                     NULL);
}
