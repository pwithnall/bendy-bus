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

#include <glib.h>

#include "dfsm-output-sequence.h"

G_DEFINE_INTERFACE (DfsmOutputSequence, dfsm_output_sequence, G_TYPE_OBJECT)

static void
dfsm_output_sequence_default_init (DfsmOutputSequenceInterface *iface)
{
	/* Nothing to see here */
}

/**
 * dfsm_output_sequence_output:
 * @self: a #DfsmOutputSequence
 * @error: a #GError, or %NULL
 *
 * Cause all events added to the #DfsmOutputSequence to be processed and acted on, in the order they were added to the sequence. If processing any
 * event fails, processing is immediately abandoned and @error is set. Any remaining events will not be processed.
 *
 * This method must be called at most once per #DfsmOutputSequence instance.
 */
void
dfsm_output_sequence_output (DfsmOutputSequence *self, GError **error)
{
	DfsmOutputSequenceInterface *iface;
	GError *child_error = NULL;

	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (self));
	g_return_if_fail (error == NULL || *error == NULL);

	iface = DFSM_OUTPUT_SEQUENCE_GET_IFACE (self);
	g_assert (iface->output != NULL);

	iface->output (self, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
	}
}

/**
 * dfsm_output_sequence_add_reply:
 * @self: a #DfsmOutputSequence
 * @parameters: out parameters from the method invocation
 *
 * Add an event to the output sequence to reply to the ongoing method call with return values given as out @parameters.
 */
void
dfsm_output_sequence_add_reply (DfsmOutputSequence *self, GVariant *parameters)
{
	DfsmOutputSequenceInterface *iface;

	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (self));
	g_return_if_fail (parameters != NULL);

	iface = DFSM_OUTPUT_SEQUENCE_GET_IFACE (self);
	g_assert (iface->add_reply != NULL);

	iface->add_reply (self, parameters);
}

/**
 * dfsm_output_sequence_add_throw:
 * @self: a #DfsmOutputSequence
 * @throw_error: #GError to be thrown
 *
 * Add an event to the output sequence to throw the given @throw_error.
 */
void
dfsm_output_sequence_add_throw (DfsmOutputSequence *self, GError *throw_error)
{
	DfsmOutputSequenceInterface *iface;

	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (self));
	g_return_if_fail (throw_error != NULL);

	iface = DFSM_OUTPUT_SEQUENCE_GET_IFACE (self);
	g_assert (iface->add_throw != NULL);

	iface->add_throw (self, throw_error);
}

/**
 * dfsm_output_sequence_add_emit:
 * @self: a #DfsmOutputSequence
 * @interface_name: name of the D-Bus interface defining @signal_name
 * @signal_name: name of the D-Bus signal to emit
 * @parameters: parameters to the signal
 *
 * Add an event to the output sequence to emit @signal_name with the given @parameters.
 */
void
dfsm_output_sequence_add_emit (DfsmOutputSequence *self, const gchar *interface_name, const gchar *signal_name, GVariant *parameters)
{
	DfsmOutputSequenceInterface *iface;

	g_return_if_fail (DFSM_IS_OUTPUT_SEQUENCE (self));
	g_return_if_fail (interface_name != NULL && *interface_name != '\0');
	g_return_if_fail (signal_name != NULL && *signal_name != '\0');
	g_return_if_fail (parameters != NULL);

	iface = DFSM_OUTPUT_SEQUENCE_GET_IFACE (self);
	g_assert (iface->add_emit != NULL);

	iface->add_emit (self, interface_name, signal_name, parameters);
}
