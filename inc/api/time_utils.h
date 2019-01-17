/**
 * @file time_utils.h
 * @brief Time management utility functions.
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * The functions declared here are meant to be used on struct timespec objects,
 * whose definition is in <time.h>.
 *
 * Inspired by the functions shown by Professor Giorgio Buttazzo during his
 * Real-Time Systems class.
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

// NOTICE: it is true that in general void functions are unsafe, because values
// specified by pointers should always be checked and a value should always be
// returned to the caller.
// On the other hand, if the value provided is invalid we can always say that
// the behavior is undefined, after all the correctness of the arguments is a
// duty of the caller.

// The following macro is needed because Intellisense in VS Code editor fails to
// find the following macros:
// CLOCK_REALTIME
// CLOCK_MONOTONIC
// and other macros defined in linux/time.h .

// However, this won't affect the system during compilation with gcc thanks to
// __INTELLISENSE__ macro.

#if defined __INTELLISENSE__
#include <linux/time.h>
#endif

#include <time.h>

/** Copies the time value in ts into td.
 */
extern void time_copy(struct timespec *td, struct timespec ts);

/** Adds ms milliseconds to the value contained in t.
 */
extern void time_add_ms(struct timespec *t, int ms);

/** Compares the two values contained in t1 and t2.
 *
 * @return The result is either
 * 		-  1	if t1 > t2
 * 		- -1	if t1 < t2
 * 		-  0	otherwise
 */
extern int time_cmp(struct timespec t1, struct timespec t2);

/** Calculates the difference between two times.
 * The value in tdest will be replaced with the difference between the two
 * timespecs (t2 - t1); of course, t2 shall be greater or equal than t1.
 *
 * @return A non zero value if t2 is not greater or equal than t1, indicating
 * the error case, zero on success.
 */
extern int time_diff(struct timespec *tdest, struct timespec t2, struct timespec t1);

#endif
