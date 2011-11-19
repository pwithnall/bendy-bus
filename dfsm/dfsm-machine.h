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

#ifndef DFSM_MACHINE_H
#define DFSM_MACHINE_H

#include <glib.h>
#include <glib-object.h>

#include "dfsm-environment.h"
#include "dfsm-utils.h"

G_BEGIN_DECLS

typedef enum {
	DFSM_SIMULATION_ERROR_INVALID_STATUS,
	DFSM_SIMULATION_ERROR_UNKNOWN_INTERFACE,
} DfsmSimulationError;

#define DFSM_SIMULATION_ERROR dfsm_simulation_error_quark ()
GQuark dfsm_simulation_error_quark (void) G_GNUC_CONST;

/**
 * DfsmSimulationStatus:
 * @DFSM_SIMULATION_STATUS_STOPPED: Simulation is not running.
 * @DFSM_SIMULATION_STATUS_STARTED: Simulation is running.
 *
 * The current status of the simulation. This is not equivalent to the current state number of the simulated DFSM.
 */
typedef enum {
	DFSM_SIMULATION_STATUS_STOPPED = 0,
	DFSM_SIMULATION_STATUS_STARTED,
} DfsmSimulationStatus;

#define DFSM_TYPE_SIMULATION_STATUS dfsm_simulation_status_get_type ()
GType dfsm_simulation_status_get_type (void) G_GNUC_CONST;

/**
 * DfsmMachineStateNumber:
 *
 * A unique identifier for a DFSM state in a given #DfsmMachine.
 */
typedef guint DfsmMachineStateNumber;

/**
 * DFSM_MACHINE_STARTING_STATE:
 *
 * The #DfsmMachineStateNumber of the starting state of any #DfsmMachine.
 */
#define DFSM_MACHINE_STARTING_STATE 0

#define DFSM_TYPE_MACHINE		(dfsm_machine_get_type ())
#define DFSM_MACHINE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), DFSM_TYPE_MACHINE, DfsmMachine))
#define DFSM_MACHINE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), DFSM_TYPE_MACHINE, DfsmMachineClass))
#define DFSM_IS_MACHINE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), DFSM_TYPE_MACHINE))
#define DFSM_IS_MACHINE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), DFSM_TYPE_MACHINE))
#define DFSM_MACHINE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), DFSM_TYPE_MACHINE, DfsmMachineClass))

typedef struct _DfsmMachinePrivate	DfsmMachinePrivate;

typedef struct {
	GObject parent;
	DfsmMachinePrivate *priv;
} DfsmMachine;

typedef struct {
	GObjectClass parent;
} DfsmMachineClass;

GType dfsm_machine_get_type (void) G_GNUC_CONST;

DfsmMachine *dfsm_machine_new (DfsmEnvironment *environment, GPtrArray/*<string>*/ *state_names,
                               GPtrArray/*<DfsmAstTransition>*/ *transitions) DFSM_CONSTRUCTOR;

void dfsm_machine_start_simulation (DfsmMachine *self);
void dfsm_machine_stop_simulation (DfsmMachine *self);

GVariant *dfsm_machine_call_method (DfsmMachine *self, const gchar *method_name, GVariant *parameters, GError **error) DFSM_CONSTRUCTOR;

DfsmEnvironment *dfsm_machine_get_environment (DfsmMachine *self) G_GNUC_PURE;

G_END_DECLS

#endif /* !DFSM_MACHINE_H */
