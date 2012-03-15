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

#ifndef DFSM_OUTPUT_SEQUENCE_H
#define DFSM_OUTPUT_SEQUENCE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define DFSM_TYPE_OUTPUT_SEQUENCE		(dfsm_output_sequence_get_type ())
#define DFSM_OUTPUT_SEQUENCE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_OUTPUT_SEQUENCE, DfsmOutputSequence))
#define DFSM_OUTPUT_SEQUENCE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_OUTPUT_SEQUENCE, DfsmOutputSequenceInterface))
#define DFSM_IS_OUTPUT_SEQUENCE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_OUTPUT_SEQUENCE))
#define DFSM_OUTPUT_SEQUENCE_GET_IFACE(o)	(G_TYPE_INSTANCE_GET_INTERFACE ((o), DFSM_TYPE_OUTPUT_SEQUENCE, DfsmOutputSequenceInterface))

/**
 * DfsmOutputSequence:
 *
 * All the fields in the #DfsmOutputSequence structure are private and should never be accessed directly.
 */
typedef struct _DfsmOutputSequence		DfsmOutputSequence; /* dummy typedef */

/**
 * DfsmOutputSequenceInterface:
 * @output: output all state-changing actions queued up in the #DfsmOutputSequence, in order, returning any errors in @error
 * @add_reply: append a D-Bus reply action with the given @parameters tuple to the sequence of actions queued up in the #DfsmOutputSequence
 * @add_throw: add a D-Bus error reply action with the given #GError domain, code and message to the sequence of actions queued up in
 * the #DfsmOutputSequence
 * @add_emit: add a D-Bus signal emission for the given @signal_name on the given @interface_name with the given @parameters tuple to the sequence of
 * actions queued up in the #DfsmOutputSequence.
 *
 * Interface structure for #DfsmOutputSequence.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	void (*output) (DfsmOutputSequence *self, GError **error);

	void (*add_reply) (DfsmOutputSequence *self, GVariant *parameters);
	void (*add_throw) (DfsmOutputSequence *self, GError *throw_error);
	void (*add_emit) (DfsmOutputSequence *self, const gchar *interface_name, const gchar *signal_name, GVariant *parameters);
} DfsmOutputSequenceInterface;

GType dfsm_output_sequence_get_type (void) G_GNUC_CONST;

void dfsm_output_sequence_output (DfsmOutputSequence *self, GError **error);

void dfsm_output_sequence_add_reply (DfsmOutputSequence *self, GVariant *parameters);
void dfsm_output_sequence_add_throw (DfsmOutputSequence *self, GError *throw_error);
void dfsm_output_sequence_add_emit (DfsmOutputSequence *self, const gchar *interface_name, const gchar *signal_name, GVariant *parameters);

G_END_DECLS

#endif /* !DFSM_OUTPUT_SEQUENCE_H */
