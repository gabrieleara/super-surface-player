#ifndef TIME_H
#define TIME_H

// Time management utility functions
// Inspired by the functions shown by Professor Giorgio Buttazzo

// TODO: the void functions are unsafe, pointers should always be checked and
// a value should always be returned to the caller.
// On the other hand, if the value provided is invalid we can always say that
// the behavior is undefined, after all the correctness of the arguments is a
// duty of the caller.

// Following macro is needed because Intellisense fails to find the following macros:
// CLOCK_REALTIME
// CLOCK_MONOTONIC
// and other macros defined in linux/time.h .

// However, this won't affect the system during compilation with gcc thanks to
// __INTELLISENSE__ macro.

#if defined __INTELLISENSE__
#include <linux/time.h>
#endif

#include <time.h>

// Copies the values in ts into td
extern void time_copy(struct timespec *td, struct timespec ts);

// Adds ms milliseconds to the value contained in t
extern void time_add_ms(struct timespec *t, int ms);

// Compares the two values contained in t1 and t2.
// Returns:
//		 1	if t1 > t2
//		-1	if t1 < t2
//		 0	otherwise
extern int time_cmp(struct timespec t1, struct timespec t2);

// The value in tdest will be replaced with the difference between the two
// timespec (t2 - t1); to work properly, t2 must be greater or equal than t1

// Returns a non zero value if t2 is not greater or equal than t1, zero if it
// succedes
extern int time_diff(struct timespec *tdest, struct timespec t2, struct timespec t1);

#endif
