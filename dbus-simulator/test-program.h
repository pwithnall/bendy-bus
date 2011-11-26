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

#include <glib.h>
#include <glib-object.h>

#include "program-wrapper.h"

#ifndef DSIM_TEST_PROGRAM_H
#define DSIM_TEST_PROGRAM_H

G_BEGIN_DECLS

#define DSIM_TYPE_TEST_PROGRAM		(dsim_test_program_get_type ())
#define DSIM_TEST_PROGRAM(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), DSIM_TYPE_TEST_PROGRAM, DsimTestProgram))
#define DSIM_TEST_PROGRAM_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), DSIM_TYPE_TEST_PROGRAM, DsimTestProgramClass))
#define DSIM_IS_TEST_PROGRAM(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DSIM_TYPE_TEST_PROGRAM))
#define DSIM_IS_TEST_PROGRAM_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DSIM_TYPE_TEST_PROGRAM))
#define DSIM_TEST_PROGRAM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DSIM_TYPE_TEST_PROGRAM, DsimTestProgramClass))

typedef struct _DsimTestProgramPrivate	DsimTestProgramPrivate;

typedef struct {
	DsimProgramWrapper parent;
	DsimTestProgramPrivate *priv;
} DsimTestProgram;

typedef struct {
	DsimProgramWrapperClass parent;
} DsimTestProgramClass;

GType dsim_test_program_get_type (void) G_GNUC_CONST;

DsimTestProgram *dsim_test_program_new (GFile *working_directory, const gchar *program_name, GPtrArray/*<string>*/ *argv,
                                        GPtrArray/*<string>*/ *envp) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS

#endif /* !DSIM_TEST_PROGRAM_H */
