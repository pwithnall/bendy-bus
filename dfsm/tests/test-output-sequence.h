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

#ifndef TEST_OUTPUT_SEQUENCE_H
#define TEST_OUTPUT_SEQUENCE_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <dfsm/dfsm-output-sequence.h>

G_BEGIN_DECLS

typedef enum {
	ENTRY_NONE,
	ENTRY_REPLY,
	ENTRY_THROW,
	ENTRY_EMIT,
} QueueEntryType;

#define TEST_TYPE_OUTPUT_SEQUENCE		(test_output_sequence_get_type ())
#define TEST_OUTPUT_SEQUENCE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_OUTPUT_SEQUENCE, TestOutputSequence))
#define TEST_OUTPUT_SEQUENCE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TEST_TYPE_OUTPUT_SEQUENCE, TestOutputSequenceClass))
#define TEST_IS_OUTPUT_SEQUENCE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TEST_TYPE_OUTPUT_SEQUENCE))
#define TEST_IS_OUTPUT_SEQUENCE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TEST_TYPE_OUTPUT_SEQUENCE))
#define TEST_OUTPUT_SEQUENCE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TEST_TYPE_OUTPUT_SEQUENCE, TestOutputSequenceClass))

typedef struct _TestOutputSequencePrivate	TestOutputSequencePrivate;

typedef struct {
	GObject parent;
	TestOutputSequencePrivate *priv;
} TestOutputSequence;

typedef struct {
	GObjectClass parent;
} TestOutputSequenceClass;

GType test_output_sequence_get_type (void) G_GNUC_CONST;

DfsmOutputSequence *test_output_sequence_new (QueueEntryType first_entry_type, ...) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS

#endif /* !TEST_OUTPUT_SEQUENCE_H */
