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

#include "test-program.h"

static void dsim_test_program_dispose (GObject *object);
static void dsim_test_program_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void dsim_test_program_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void dsim_test_program_build_argv (DsimProgramWrapper *wrapper, GPtrArray *argv);
static void dsim_test_program_build_envp (DsimProgramWrapper *wrapper, GPtrArray *envp);

struct _DsimTestProgramPrivate {
	GPtrArray/*<string>*/ *argv;
	GPtrArray/*<string>*/ *envp;
};

enum {
	PROP_ARGV = 1,
	PROP_ENVP,
};

G_DEFINE_TYPE (DsimTestProgram, dsim_test_program, DSIM_TYPE_PROGRAM_WRAPPER)

static void
dsim_test_program_class_init (DsimTestProgramClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	DsimProgramWrapperClass *wrapper_class = DSIM_PROGRAM_WRAPPER_CLASS (klass);

	g_type_class_add_private (klass, sizeof (DsimTestProgramPrivate));

	gobject_class->get_property = dsim_test_program_get_property;
	gobject_class->set_property = dsim_test_program_set_property;
	gobject_class->dispose = dsim_test_program_dispose;

	wrapper_class->build_argv = dsim_test_program_build_argv;
	wrapper_class->build_envp = dsim_test_program_build_envp;

	/**
	 * DsimTestProgram:argv:
	 *
	 * Vector of arguments to pass to the test program. This may be empty, but must not be %NULL. None of the array entries may be %NULL (i.e. the
	 * array should not be %NULL terminated).
	 */
	g_object_class_install_property (gobject_class, PROP_ARGV,
	                                 g_param_spec_boxed ("argv",
	                                                     "Argument vector", "Vector of arguments to pass to the test program.",
	                                                     G_TYPE_PTR_ARRAY,
	                                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * DsimTestProgram:envp:
	 *
	 * Vector of environment variable pairs to pass to the test program. This may be empty, but must not be %NULL. Each array entry is a string
	 * of the form <code class="literal"><replaceable>KEY</replaceable>=<replaceable>value</replaceable></code>. None of the array entries may be
	 * %NULL (i.e. the array should not be %NULL terminated).
	 */
	g_object_class_install_property (gobject_class, PROP_ENVP,
	                                 g_param_spec_boxed ("envp",
	                                                     "Environment pairs", "Vector of environment variable pairs to pass to the test program.",
	                                                     G_TYPE_PTR_ARRAY,
	                                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
dsim_test_program_init (DsimTestProgram *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, DSIM_TYPE_TEST_PROGRAM, DsimTestProgramPrivate);
}

static void
dsim_test_program_dispose (GObject *object)
{
	DsimTestProgramPrivate *priv = DSIM_TEST_PROGRAM (object)->priv;

	if (priv->argv != NULL) {
		g_ptr_array_unref (priv->argv);
		priv->argv = NULL;
	}

	if (priv->envp != NULL) {
		g_ptr_array_unref (priv->envp);
		priv->envp = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (dsim_test_program_parent_class)->dispose (object);
}

static void
dsim_test_program_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	DsimTestProgramPrivate *priv = DSIM_TEST_PROGRAM (object)->priv;

	switch (property_id) {
		case PROP_ARGV:
			g_value_set_boxed (value, priv->argv);
			break;
		case PROP_ENVP:
			g_value_set_boxed (value, priv->envp);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dsim_test_program_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	DsimTestProgramPrivate *priv = DSIM_TEST_PROGRAM (object)->priv;

	switch (property_id) {
		case PROP_ARGV:
			/* Construct-only */
			priv->argv = g_ptr_array_ref (g_value_get_boxed (value));
			break;
		case PROP_ENVP:
			/* Construct-only */
			priv->envp = g_ptr_array_ref (g_value_get_boxed (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
dsim_test_program_build_argv (DsimProgramWrapper *wrapper, GPtrArray *argv)
{
	DsimTestProgramPrivate *priv = DSIM_TEST_PROGRAM (wrapper)->priv;
	guint i;

	/* Copy parameters. */
	for (i = 0; i < priv->argv->len; i++) {
		const gchar *arg = (const gchar*) g_ptr_array_index (priv->argv, i);

		g_assert (arg != NULL);
		g_ptr_array_add (argv, g_strdup (arg));
	}
}

static void
dsim_test_program_build_envp (DsimProgramWrapper *wrapper, GPtrArray *envp)
{
	DsimTestProgramPrivate *priv = DSIM_TEST_PROGRAM (wrapper)->priv;
	guint i;

	/* Copy pairs. */
	for (i = 0; i < priv->envp->len; i++) {
		const gchar *pair = (const gchar*) g_ptr_array_index (priv->envp, i);

		g_assert (pair != NULL);
		g_ptr_array_add (envp, g_strdup (pair));
	}
}

/**
 * dsim_test_program_new:
 * @working_directory: directory to start the test program in
 * @program_name: name of the executable to run
 * @argv: (allow-none): array of non-%NULL strings to pass as an argument vector to the program, or %NULL to pass no arguments
 * @envp: (allow-none): array of non-%NULL key–value pair strings to use as the environment for the program, or %NULL to use an empty environment
 *
 * Creates a new #DsimTestProgram, but does not spawn the program yet. dsim_program_wrapper_spawn() does that.
 *
 * Return value: (transfer full): a new #DsimTestProgram
 */
DsimTestProgram *
dsim_test_program_new (GFile *working_directory, const gchar *program_name, GPtrArray/*<string>*/ *argv, GPtrArray/*<string>*/ *envp)
{
	DsimTestProgram *program;

	g_return_val_if_fail (G_IS_FILE (working_directory), NULL);
	g_return_val_if_fail (program_name != NULL && *program_name != '\0', NULL);

	if (argv == NULL) {
		/* Empty argument vector. */
		argv = g_ptr_array_new_with_free_func (g_free);
	} else {
		g_ptr_array_ref (argv);
	}

	if (envp == NULL) {
		/* Empty environment vector. */
		envp = g_ptr_array_new_with_free_func (g_free);
	} else {
		g_ptr_array_ref (envp);
	}

	program = g_object_new (DSIM_TYPE_TEST_PROGRAM,
	                        "working-directory", working_directory,
	                        "program-name", program_name,
	                        "argv", argv,
	                        "envp", envp,
	                        NULL);

	g_ptr_array_unref (envp);
	g_ptr_array_unref (argv);

	return program;
}
