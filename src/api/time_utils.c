/**
 * @file time_utils.c
 * @brief Time management utility functions.
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * Implementation of the functions declared in api/time_utils.h.
 * For documentation, see the corresponding header file.
 *
 * Inspired by the functions shown by Professor Giorgio Buttazzo during his
 * Real-Time Systems class.
 */

#include "api/time_utils.h"

void time_copy(struct timespec *td, struct timespec ts)
{
	td->tv_sec = ts.tv_sec;
	td->tv_nsec = ts.tv_nsec;
}

void time_add_ms(struct timespec *t, int ms)
{
	t->tv_sec += ms/1000;
	t->tv_nsec += (ms%1000)*1000000;
	if (t->tv_nsec > 1000000000)
	{
		t->tv_nsec -= 1000000000;
		t->tv_sec += 1;
	}
}

int time_cmp(struct timespec t1, struct timespec t2)
{
	if (t1.tv_sec > t2.tv_sec) return 1;
	if (t1.tv_sec < t2.tv_sec) return -1;
	if (t1.tv_nsec > t2.tv_nsec) return 1;
	if (t1.tv_nsec < t2.tv_nsec) return -1;
	return 0;
}

int time_diff(struct timespec *tdest, struct timespec t2, struct timespec t1)
{
	if (time_cmp(t2, t1) < 0)
		return -1;

	tdest->tv_sec = t2.tv_sec - t1.tv_sec;
	tdest->tv_nsec = t2.tv_nsec - t1.tv_nsec;

	if (tdest->tv_nsec < 0)
	{
		tdest->tv_nsec = 1000000000 - tdest->tv_nsec;
		tdest->tv_sec -= 1;
	}

	return 0;
}
