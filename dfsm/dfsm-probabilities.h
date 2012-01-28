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

#include <float.h>
#include <glib.h>

#ifndef DFSM_PROBABILITIES_H
#define DFSM_PROBABILITIES_H

G_BEGIN_DECLS

/**
 * DFSM_BIASED_COIN_FLIP:
 * @p: probability of success (in the range [0..1.0])
 *
 * Perform a single biased coin flip with probability of success @p.
 *
 * Return value: %TRUE with probability @p, %FALSE otherwise
 */
#define DFSM_BIASED_COIN_FLIP(p) (g_random_int () < G_MAXUINT32 * CLAMP ((gdouble) (p), 0.0, 1.0))

#define _DFSM_DISTRIBUTION_SEQ(N, OP, TERM, ...) _DFSM_DISTRIBUTION_SEQ##N(OP, TERM, __VA_ARGS__)
#define _DFSM_DISTRIBUTION_SEQ1(OP, TERM, first_name, first_p) TERM(first_name)
#define _DFSM_DISTRIBUTION_SEQ2(OP, TERM, first_name, first_p, ...) OP(first_name, _DFSM_DISTRIBUTION_SEQ1(OP, TERM, __VA_ARGS__))
#define _DFSM_DISTRIBUTION_SEQ3(OP, TERM, first_name, first_p, ...) OP(first_name, _DFSM_DISTRIBUTION_SEQ2(OP, TERM, __VA_ARGS__))
#define _DFSM_DISTRIBUTION_SEQ4(OP, TERM, first_name, first_p, ...) OP(first_name, _DFSM_DISTRIBUTION_SEQ3(OP, TERM, __VA_ARGS__))
#define _DFSM_DISTRIBUTION_SEQ5(OP, TERM, first_name, first_p, ...) OP(first_name, _DFSM_DISTRIBUTION_SEQ4(OP, TERM, __VA_ARGS__))
#define _DFSM_DISTRIBUTION_SEQ6(OP, TERM, first_name, first_p, ...) OP(first_name, _DFSM_DISTRIBUTION_SEQ5(OP, TERM, __VA_ARGS__))
#define _DFSM_DISTRIBUTION_SEQ7(OP, TERM, first_name, first_p, ...) OP(first_name, _DFSM_DISTRIBUTION_SEQ6(OP, TERM, __VA_ARGS__))

#define _DFSM_DISTRIBUTION_NOOP(A) A

#define _DFSM_DISTRIBUTION_LIST_(A, B) A, B
#define _DFSM_DISTRIBUTION_LIST(N, ...) _DFSM_DISTRIBUTION_SEQ(N, _DFSM_DISTRIBUTION_LIST_, _DFSM_DISTRIBUTION_NOOP, __VA_ARGS__)

#define _DFSM_DISTRIBUTION_LIST_UINT32_(A, B) A * G_MAXUINT32, B
#define _DFSM_DISTRIBUTION_LIST_UINT32__(A) A * G_MAXUINT32
#define _DFSM_DISTRIBUTION_LIST_UINT32(N, ...) _DFSM_DISTRIBUTION_SEQ(N, _DFSM_DISTRIBUTION_LIST_UINT32_, _DFSM_DISTRIBUTION_LIST_UINT32__, __VA_ARGS__)

#define _DFSM_DISTRIBUTION_SUM_(A, B) A + B
#define _DFSM_DISTRIBUTION_SUM(N, ...) _DFSM_DISTRIBUTION_SEQ(N, _DFSM_DISTRIBUTION_SUM_, _DFSM_DISTRIBUTION_NOOP, __VA_ARGS__)

/**
 * DFSM_NONUNIFORM_DISTRIBUTION:
 * @N: number of intervals in the distribution
 * @first_name: name of the first interval
 * @...: probability of the first interval being chosen, followed by more interval-name–probability pairs
 *
 * Define a non-uniform continuous probability distribution with @N intervals given by @first_name and @..., choose a random interval in the distribution
 * and execute code corresponding to it.
 *
 * This macro opens a switch statement between the different possible intervals. Calling code should provide all the necessary case statements (but not
 * a default case statement), then use the %DFSM_NONUNIFORM_DISTRIBUTION_END macro to close the block.
 */
#define DFSM_NONUNIFORM_DISTRIBUTION(N, first_name, ...) { \
	enum TempEnum { \
		_DFSM_DISTRIBUTION_LIST(N, first_name, __VA_ARGS__) \
	}; \
	guint32 intervals[] = { \
		_DFSM_DISTRIBUTION_LIST_UINT32(N, __VA_ARGS__,) \
	}; \
\
	gdouble diff = (_DFSM_DISTRIBUTION_SUM(N, __VA_ARGS__,)) - 1.0; \
	G_STATIC_ASSERT (diff < DBL_EPSILON && -diff > DBL_EPSILON); \
\
	switch ((enum TempEnum) dfsm_random_nonuniform_distribution (intervals, N)) { \
		default: \
			g_assert_not_reached (); \

/**
 * DFSM_NONUNIFORM_DISTRIBUTION_END:
 *
 * Close a non-uniform continuous probability distribution switch statement started with %DFSM_NONUNIFORM_DISTRIBUTION.
 */
#define DFSM_NONUNIFORM_DISTRIBUTION_END \
	} \
}

G_GNUC_INTERNAL guint dfsm_random_nonuniform_distribution (guint32 intervals[], gsize intervals_len);

G_END_DECLS

#endif /* !DFSM_PROBABILITIES_H */
