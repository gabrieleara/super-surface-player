// Time management utility functions
// Inspired by the functions shown by Professor Giorgio Buttazzo

// TODO: the void functions are unsafe, pointers should always be checked and
// a value should always be returned to the caller.
// On the other hand, if the value provided is invalid we can always say that
// the behavior is undefined, after all the correctness of the arguments is a
// duty of the caller.

#include <time.h>


// Copies the values in ts into td
void time_copy(struct timespec *td, struct timespec ts)
{
	td->tv_sec = ts.tv_sec;
	td->tv_nsec = ts.tv_nsec;
}

// Adds ms milliseconds to the value contained in t
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

// Compares the two values contained in t1 and t2.
// Returns:
//		 1	if t1 > t2
//		-1	if t1 < t2
//		 0	otherwise
int time_cmp(struct timespec t1, struct timespec t2)
{
	if (t1.tv_sec > t2.tv_sec) return 1;
	if (t1.tv_sec < t2.tv_sec) return -1;
	if (t1.tv_nsec > t2.tv_nsec) return 1;
	if (t1.tv_nsec < t2.tv_nsec) return -1;
	return 0;
}


// The value in tdest will be replaced with the difference between the two
// timespec (t2 - t1); to work properly, t2 must be greater or equal than t1

// Returns a non zero value if t2 is not greater or equal than t1, zero if it
// succedes
int time_diff(struct timespec *tdest, struct timespec t2, struct timespec t1) {
	if(time_cmp(t2, t1) < 0)
		return -1;

	tdest->tv_sec = t2.tv_sec - t1.tv_sec;
	tdest->tv_nsec = t2.tv_nsec - t1.tv_nsec;

	if(tdest->tv_nsec < 0) {
		tdest->tv_nsec = 1000000000 - tdest->tv_nsec;
		tdest->tv_sec -= 1;
	}

	return 0;
}
