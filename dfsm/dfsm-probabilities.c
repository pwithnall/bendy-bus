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

#include "dfsm-probabilities.h"

/**
 * dfsm_random_nonuniform_distribution:
 * @intervals: (array length=intervals_len): list of intervals in the distribution
 * @intervals_len: number of elements in @intervals
 *
 * Randomly choose an interval from those given in @intervals, forming a non-uniform distribution. The elements of @intervals must sum to %G_MAXUINT32,
 * giving a distribution over [0..%G_MAXUINT32]. The probability of returning a given interval from the distribution is proportional to the interval's
 * size.
 *
 * This function is intended to be used through the %DFSM_NONUNIFORM_DISTRIBUTION macro, which performs additional compile-time checks and adds
 * syntactic sugar.
 *
 * Return value: the index of a randomly chosen interval out of the given @intervals, in the range [0..%G_MAXUINT32]
 */
guint
dfsm_random_nonuniform_distribution (guint32 intervals[], gsize intervals_len)
{
	guint32 rnd;
	guint i;

	/* We assume that SUM(intervals) == G_MAXUINT32. This is checked (statically) by macros such as DFSM_NONUNIFORM_DISTRIBUTION. */
	g_return_val_if_fail (intervals_len > 0, 0);

	/* Choose a random integer in the range [0..2^{32}-1] and loop through the intervals until we find the interval it lies in.
	 * We use g_random_int() for a full 32 bits of randomness even though we probably only use a couple of bits of randomness. This isn't a
	 * problem, since we're only using a PRNG, not an actual entropy pool. */
	for (rnd = g_random_int (), i = 0; rnd > intervals[i] && i < intervals_len; rnd -= intervals[i], i++) {
		;
	}

	/* Sanity check. */
	g_assert (i < intervals_len);

	return i;
}
