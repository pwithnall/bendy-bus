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

#ifndef DSIM_PROGRAM_WRAPPER_H
#define DSIM_PROGRAM_WRAPPER_H

G_BEGIN_DECLS

#define DSIM_TYPE_PROGRAM_WRAPPER		(dsim_program_wrapper_get_type ())
#define DSIM_PROGRAM_WRAPPER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DSIM_TYPE_PROGRAM_WRAPPER, DsimProgramWrapper))
#define DSIM_PROGRAM_WRAPPER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DSIM_TYPE_PROGRAM_WRAPPER, DsimProgramWrapperClass))
#define DSIM_IS_PROGRAM_WRAPPER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DSIM_TYPE_PROGRAM_WRAPPER))
#define DSIM_IS_PROGRAM_WRAPPER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DSIM_TYPE_PROGRAM_WRAPPER))
#define DSIM_PROGRAM_WRAPPER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DSIM_TYPE_PROGRAM_WRAPPER, DsimProgramWrapperClass))

typedef struct _DsimProgramWrapperPrivate	DsimProgramWrapperPrivate;

typedef struct {
	GObject parent;
	DsimProgramWrapperPrivate *priv;
} DsimProgramWrapper;

typedef struct {
	GObjectClass parent;

	/* Abstract functions */
	void (*build_argv) (DsimProgramWrapper *self, GPtrArray *argv); /* mandatory to implement */
	void (*build_envp) (DsimProgramWrapper *self, GPtrArray *envp); /* optional to implement */

	/* Signal handlers */
	gboolean (*spawn_begin) (DsimProgramWrapper *self, GError **error);
	void (*spawn_end) (DsimProgramWrapper *self, GPid pid);
	void (*process_died) (DsimProgramWrapper *self, gint status);
} DsimProgramWrapperClass;

GType dsim_program_wrapper_get_type (void) G_GNUC_CONST;

void dsim_program_wrapper_spawn (DsimProgramWrapper *self, GError **error);
void dsim_program_wrapper_kill (DsimProgramWrapper *self);

GFile *dsim_program_wrapper_get_working_directory (DsimProgramWrapper *self) G_GNUC_PURE;
GPid dsim_program_wrapper_get_process_id (DsimProgramWrapper *self);
const gchar *dsim_program_wrapper_get_logging_domain_name (DsimProgramWrapper *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DSIM_PROGRAM_WRAPPER_H */
