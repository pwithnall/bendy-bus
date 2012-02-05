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

#include <math.h>

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

/* NOTE: These aren't thread safe. */
static gdouble normal_z1_mu = 0.0;
static gdouble normal_z1_sigma = 0.0;
static gdouble normal_z1 = 0.0;

/**
 * dfsm_random_normal_distribution:
 * @mu: mean of the distribution to sample from
 * @sigma: standard deviation of the distribution to sample from
 *
 * Randomly choose a value from the normal distribution parametrised by standard deviation @sigma and mean @mu. If @sigma is
 * <code class="literal">1.0</code> and @mu is <code class="literal">0.0</code>, this is the standard normal distribution.
 *
 * This is implemented using the polar Box–Muller transform, and as such generates two values from the same distribution simultaneously, and will
 * cache one until the next time dfsm_random_normal_distribution() is called. Consequently, it is faster to re-use the same @sigma and @mu between
 * consecutive calls to dfsm_random_normal_distribution() than to change their values.
 *
 * This function is not thread safe.
 *
 * Return value: a random value from the normal distribution parametrised by @sigma and @mu
 */
gdouble
dfsm_random_normal_distribution (gdouble mu, gdouble sigma)
{
	gdouble u, v, s, r;

	/* If we have a result left over from the previous calculation, return that. This isn't thread safe. */
	if (normal_z1_sigma == sigma && normal_z1_mu == mu) {
		/* Invalidate the cached value. */
		normal_z1_sigma = 0.0;
		normal_z1_mu = 0.0;

		return normal_z1;
	}

	/* Use the Box–Muller transform to generate two standard normal variables.
	 * See: http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform#Polar_form */
	do {
		u = g_random_double_range (-1.0, 1.0);
		v = g_random_double_range (-1.0, 1.0);

		s = u * u + v * v;
	} while (s == 0.0 || s == -0.0 || s >= 1.0);

	r = sqrt ((-2.0 * log (s)) / s);

	/* Calculate and cache the second value (z1). */
	normal_z1 = (u * r) * sigma + mu;
	normal_z1_mu = mu;
	normal_z1_sigma = sigma;

	return (v * r) * sigma + mu;
}
