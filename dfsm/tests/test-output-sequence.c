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

#include <string.h>
#include <glib.h>

#include "test-output-sequence.h"

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
		case ENTRY_NONE:
		default:
			g_assert_not_reached ();
	}

	g_slice_free (QueueEntry, entry);
}

static void test_output_sequence_iface_init (DfsmOutputSequenceInterface *iface);

static void test_output_sequence_output (DfsmOutputSequence *sequence, GError **error);
static void test_output_sequence_add_reply (DfsmOutputSequence *sequence, GVariant *parameters);
static void test_output_sequence_add_throw (DfsmOutputSequence *sequence, GError *throw_error);
static void test_output_sequence_add_emit (DfsmOutputSequence *sequence, const gchar *interface_name, const gchar *signal_name, GVariant *parameters);

struct _TestOutputSequencePrivate {
	GQueue/*<QueueEntry>*/ expected_queue; /* head is the oldest entry (i.e. the one to get executed first) */
};

G_DEFINE_TYPE_EXTENDED (TestOutputSequence, test_output_sequence, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (DFSM_TYPE_OUTPUT_SEQUENCE, test_output_sequence_iface_init))

static void
test_output_sequence_class_init (TestOutputSequenceClass *klass)
{
	g_type_class_add_private (klass, sizeof (TestOutputSequencePrivate));
}

static void
test_output_sequence_init (TestOutputSequence *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TEST_TYPE_OUTPUT_SEQUENCE, TestOutputSequencePrivate);

	/* Initialise the queue. */
	g_queue_init (&self->priv->expected_queue);
}

static void
test_output_sequence_iface_init (DfsmOutputSequenceInterface *iface)
{
	iface->output = test_output_sequence_output;
	iface->add_reply = test_output_sequence_add_reply;
	iface->add_throw = test_output_sequence_add_throw;
	iface->add_emit = test_output_sequence_add_emit;
}

static void
test_output_sequence_output (DfsmOutputSequence *sequence, GError **error)
{
	TestOutputSequencePrivate *priv = TEST_OUTPUT_SEQUENCE (sequence)->priv;

	/* Assert there are no entries left in the expected queue. */
	g_assert (g_queue_is_empty (&priv->expected_queue) == TRUE);
}

static void
test_output_sequence_add_reply (DfsmOutputSequence *sequence, GVariant *parameters)
{
	TestOutputSequencePrivate *priv = TEST_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;

	/* Pop an entry off the head of the expected queue and compare it to the incoming entry. */
	queue_entry = g_queue_pop_head (&priv->expected_queue);
	g_assert (queue_entry != NULL);

	/* Compare the entries. */
	g_assert_cmpuint (queue_entry->entry_type, ==, ENTRY_REPLY);
	g_assert (g_variant_equal (queue_entry->reply.parameters, parameters) == TRUE);

	queue_entry_free (queue_entry);
}

static void
test_output_sequence_add_throw (DfsmOutputSequence *sequence, GError *throw_error)
{
	TestOutputSequencePrivate *priv = TEST_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;

	/* Pop an entry off the head of the expected queue and compare it to the incoming entry. */
	queue_entry = g_queue_pop_head (&priv->expected_queue);
	g_assert (queue_entry != NULL);

	/* Compare the entries. */
	g_assert_cmpuint (queue_entry->entry_type, ==, ENTRY_THROW);
	g_assert_error (queue_entry->throw.error, throw_error->domain, throw_error->code);

	queue_entry_free (queue_entry);
}

static void
test_output_sequence_add_emit (DfsmOutputSequence *sequence, const gchar *interface_name, const gchar *signal_name, GVariant *parameters)
{
	TestOutputSequencePrivate *priv = TEST_OUTPUT_SEQUENCE (sequence)->priv;
	QueueEntry *queue_entry;

	/* Pop an entry off the head of the expected queue and compare it to the incoming entry. */
	queue_entry = g_queue_pop_head (&priv->expected_queue);
	g_assert (queue_entry != NULL);

	/* Compare the entries. */
	g_assert_cmpuint (queue_entry->entry_type, ==, ENTRY_EMIT);
	g_assert_cmpstr (queue_entry->emit.interface_name, ==, interface_name);
	g_assert_cmpstr (queue_entry->emit.signal_name, ==, signal_name);
	g_assert (g_variant_equal (queue_entry->emit.parameters, parameters) == TRUE);

	queue_entry_free (queue_entry);
}

DfsmOutputSequence *
test_output_sequence_new (QueueEntryType first_entry_type, ...)
{
	TestOutputSequence *output_sequence;
	va_list ap;
	QueueEntryType entry_type;

	output_sequence = g_object_new (TEST_TYPE_OUTPUT_SEQUENCE, NULL);

	va_start (ap, first_entry_type);

	for (entry_type = first_entry_type; entry_type != ENTRY_NONE; entry_type = va_arg (ap, QueueEntryType)) {
		QueueEntry *queue_entry;

		queue_entry = g_slice_new (QueueEntry);
		queue_entry->entry_type = entry_type;

		switch (entry_type) {
			case ENTRY_REPLY:
				queue_entry->reply.parameters = g_variant_ref_sink (va_arg (ap, GVariant*));

				break;
			case ENTRY_THROW:
				queue_entry->throw.error = g_error_copy (va_arg (ap, GError*));

				break;
			case ENTRY_EMIT:
				queue_entry->emit.interface_name = g_strdup (va_arg (ap, gchar*));
				queue_entry->emit.signal_name = g_strdup (va_arg (ap, gchar*));
				queue_entry->emit.parameters = g_variant_ref_sink (va_arg (ap, GVariant*));

				break;
			case ENTRY_NONE:
			default:
				g_assert_not_reached ();
		}

		g_queue_push_tail (&output_sequence->priv->expected_queue, queue_entry);
	}

	va_end (ap);

	return DFSM_OUTPUT_SEQUENCE (output_sequence);
}
