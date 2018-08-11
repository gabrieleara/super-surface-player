#ifndef PTASKS_H
#define PTASKS_H

// Periodic tasks utility functions
// Inspired by the functions shown by Professor Giorgio Buttazzo

// Following macro is needed because Intellisense fails to find the following macros:
// PTHREAD_PRIO_NONE,
// PTHREAD_PRIO_INHERIT,
// PTHREAD_PRIO_PROTECT
// and other macros defined if __USE_POSIX199506 macro is defined.

// However, this won't affect the system during compilation with gcc thanks to
// __INTELLISENSE__ macro.

#if defined __INTELLISENSE__ && !defined __USE_POSIX199506
#define __USE_POSIX199506
#endif

#include <pthread.h>
#include <sched.h>

//-------------------------------------------------------------
// DEFINES AND DATA TYPES
//-------------------------------------------------------------

// TODO: free and allocated task ids

#define PTASK_MAX	(50)

	typedef enum __PTASK_ENUM {
		PS_ERROR = -1,
		PS_FREE = 0,
		PS_NEW,
		//PS_ACTIVE,
		PS_JOINABLE
	} ptask_state_t;

typedef struct __PTASK_STRUCT
{
	int id;				// identificator of a ptask
	long wcet;			// worst case execution time (in us): 0 means unknown
	int period;			// in milliseconds
	int deadline;		// relative to activation time (in ms)
	int priority;		// value between [0,99], standard should be in [0, 32]
	int dmiss;			// no. of occurred deadline misses
	struct timespec at;	// next activ. time
	struct timespec dl;	// next abs. deadline

	ptask_state_t _state;// state of the ptask, see ptask_state_t

	pthread_t _tid;		// pthread id of the task
	pthread_attr_t _attr;// pthread params of the task
} ptask_t;


typedef pthread_mutex_t ptask_mutex_t;
typedef pthread_cond_t ptask_cond_t;


#define PTASK_CAB_MAX		(50)	// maximum number of cabs per process
#define PTASK_CAB_MAX_SIZE	(10)	// maximum number of buffers inside a cab

typedef struct __PTASK_CAB
{
	int	id;							// identificator of the cab
	int num_buffers;				// number of buffers in the cab
	int size_buffers;				// size of each buffer (all equal)

	void *buffers[PTASK_CAB_MAX_SIZE];
									// pointers to the actual buffers

	int busy[PTASK_CAB_MAX];		// number of tasks using each buffer
	int last_index;					// pointer to the current last data buffer

	ptask_mutex_t _mux;				// mutex semaphore used to access the cab
} ptask_cab_t;

typedef void* (*ptask_body_t) (void*);
typedef int ptask_cab_id_t;

//-------------------------------------------------------------
// LIBRARY PUBLIC FUNCTIONS
//-------------------------------------------------------------

// NOTE: to avoid concurrecy problems, the thread functions are unsafe for
// their argument values, i.e. no checks are executed over pointers passed by
// argument. Deal with it. If the caller uses properly the library, as shown
// below no problems can arise.

// Thus, the following functions can only be invocated by the main thread

/*!
	Sets the scheduler to the given value. Accepted values are
	- SCHED_OTHER
	- SCHED_RR
	- SCHED_FIFO
	- SCHED_DEADLINE (currently not supported, returns EINVAL)

	All other values will make this function fail, returning EINVAL. On success
	this function returns 0.

	NOTE: Scheduler should only be set once per process execution, before all
	other threads than the main thread have been started.
*/
extern int ptask_set_scheduler(int scheduler);

/*!
	Initializes a new ptask; if no new ptasks can be initialized it returns
	EAGAIN.

	Returns zero otherwise.

	NOTICE: on success this function overwrites any value it is inside the given
	ptask_t element.
*/
extern int ptask_init(ptask_t *ptask);

/*!
	Sets each of the indicated parameters of a given ptask.
	Returns 0 on success, a non zero value otherwise.

	NOTE: this function can be called only before starting the ptask.
*/
extern int ptask_set_params(ptask_t *ptask, long wcet, int period, int deadline,
	int priority);

/*!
	Creates a new (previously initialized) ptask and starts its execution with
	the given body function.
	Returns 0 on success, a non zero value otherwise.
*/
extern int ptask_create(ptask_t *ptask, ptask_body_t body);

/*!
	Destroys a previously initialied ptask but only if the ptask_create
	function previously failed.
	Returns 0 on success, a non zero value otherwise.
*/
extern int ptask_destroy(ptask_t *ptask);

/*!
	Shorthand for the creation of a task.
	Returns 0 on success, a non zero value otherwise.
*/
extern int ptask_short(ptask_t *ptask,
	long wcet, int period, int deadline, int priority, ptask_body_t body);

/*!
	Cancels a previously started ptask, if it is still running.
	Returns 0 on success, a non zero value otherwise.

	NOTE: this function creates only a cancelation request, thus is not
	blocking. To be sure that a ptask is actually terminated call ptask_join.

	NOTE: this function may lead global values to inconsistency and should not
	be used unless the program has to be forcefully terminated.
*/
extern int ptask_cancel(ptask_t *ptask);

/*!
	Waits until a ptask has terminated its execution.
	Returns 0 on success, a non zero value otherwise.
*/
extern int ptask_join(ptask_t *ptask);

// The following functions should be called by the task itself

/*!
	Reads the current time and computes the next activation time and the
	absolute deadline of the task.
*/
extern void ptask_start_period(ptask_t *ptask);

/*!
	Suspends the calling thread until the next activation and, when awaken,
	updates activation time and deadline.

	NOTE: Even though the thread calls time_add_ms() after the wake‐up time, the
	computation is correct.
*/
extern void ptask_wait_for_period(ptask_t *ptask);

/*!
	If the thread is still in execution after its deadline, it increments the
	value of dmiss and returns a non zero value, otherwise returns zero.
*/
extern int ptask_deadline_miss(ptask_t *ptask);

/*!
	Getters for the ptask attributes
*/
extern int ptask_get_id(ptask_t *ptask);
extern long ptask_get_wcet(ptask_t *ptask);
extern int ptask_get_period(ptask_t *ptask);
extern int ptask_get_dealine(ptask_t *ptask);
extern int ptask_get_priority(ptask_t *ptask);
extern int ptask_get_dmiss(ptask_t *ptask);


//-------------------------------------------------------------
// MUTEXES AND CONDITION VARIABLES
//-------------------------------------------------------------

// The following functions are wrappers for the corresponding functions of the
// pthread library, see them for documentation
extern int ptask_mutex_init(ptask_mutex_t *mux_p);
extern int ptask_mutex_lock(ptask_mutex_t *mux_p);
extern int ptask_mutex_unlock(ptask_mutex_t *mux_p);
extern int ptask_cond_init(ptask_cond_t *cond_p);
extern int ptask_cond_wait(ptask_cond_t *cond_p, ptask_mutex_t *mux_p);
extern int ptask_cond_signal(ptask_cond_t *cond_p);
extern int ptask_cond_broadcast(ptask_cond_t *cond_p);


//-------------------------------------------------------------
// CYCLIC ASYNCHRONOUS BUFFERS
//-------------------------------------------------------------

/*!
	Initiaizes a new cab, with the given n buffers, each with the given size.

	Returns zero on success, a non zero value otherwise. In particular, if a new
	cab cannot be initialized in this moment the
*/
extern int ptask_cab_init(ptask_cab_t *ptask_cab, int n, int size, void *buffers[]);

/*!
	Resets the cab last value to no value, as it was just been initialized.
	Any task using a cab buffer can continue using it without any problem.

	Returns zero on success, a non zero value otherwise.

	NOTE: Make sure that all the readers released their reading buffers before a
	new value is inserted into the cab.
*/
extern int ptask_cab_reset(ptask_cab_t *ptask_cab);


// NOTICE: the following functions assume that the cab is used correctly, hence
// only n-1 tasks can use the cab.

/*!
	This function reserves a cab for writing purposes.
	On success, the buffer pointer given by argument will contain a pointer to
	the reserved buffer and the b_id pointer will contain the id of the said
	buffer. This id will be used later to commit any changes applied to the
	buffer using ptask_cab_putmes.

	It returns zero on success, a non zero value otherwise.

	NOTE: Attempting to reserve a buffer using a non already initialized cab
	results in undefined behavior.
*/
extern int ptask_cab_reserve(ptask_cab_t *ptask_cab, void* buffer[], ptask_cab_id_t *b_id);

/*!
	This function commits the value written in a cab buffer as the new last
	message put in the cab. It MUST be called after a ptask_cab_reserve call
	using the b_id value obtained by it and ONLY ONCE.

	On success, the given buffer is set as the contenitor of the last message in
	the cab.

	It returns zero on success, a non zero value otherwise.

	NOTE: Attempting to put a message inside a non already initialized cab
	results in undefined behavior.

	NOTE: Attempting to put a message inside a non previously reserved buffer
	may lead to inconsistency if the buffer is actually being used by only one
	ptask in readig mode.
*/
extern int ptask_cab_putmes(ptask_cab_t *ptask_cab, ptask_cab_id_t b_id);


/*!
	This function reserves a cab for reading purposes.
	On success, the buffer pointer given by argument will contain a pointer to
	the reserved buffer and the b_id pointer will contain the id of the said
	buffer. This id will be used later to release the obtained buffer using
	ptask_cab_unget.

	It returns zero on success, a non zero value otherwise. In particular,
	EAGAIN is returned if no value has been put inside the cab or the cab has
	been resetted using ptask_cab_reset.

	NOTE: Attempting to reserve a buffer using a non already initialized cab
	results in undefined behavior.
*/
extern int ptask_cab_getmes(ptask_cab_t *ptask_cab, void* buffer[],
	ptask_cab_id_t *b_id);

/*!
	This function releases a buffer acquired for reading purposes using the
	ptask_cab_getmes. It MUST be called after a ptask_cab_getmes call
	using the b_id value obtained by it and ONLY ONCE.

	It returns zero on success, a non zero value otherwise.

	NOTE: Attempting to release a buffer within a non already initialized cab
	results in undefined behavior.
*/
extern int ptask_cab_unget(ptask_cab_t *ptask_cab, ptask_cab_id_t b_id);

/*!
	UNSAFE

	Destroys a given cab.

	Shouldn't be used at all. To erase all the data within a cab in a safe way
	see ptask_cab_reset. A cab is automatically released after program
	termination, so this function isn't actually needed at all and should be
	avoided.

	Returns zero on sucess, a non zero value otherwise.

	NOTE: It has so many unsafe behaviors that they won't be listed here, but
	notice that inconsistency of data is a big deal.

	AGAIN DO NOT USE. IT'S A VERY BAD IDEA. I SHOULDN'T EVEN HAVE WRITTEN THIS
	AND IT WILL PROBABLY BE DELETED IN FUTURE REVISIONS.
*/
extern int ptask_cab_destroy(ptask_cab_t *ptask_cab);


/*

// -----------------------------------------------------------------------------
//                             TASK EXAMPLE
// -----------------------------------------------------------------------------
// Structure of a task, its argument is the pointer to the previously defined
// descriptor structure

void* task_body(void* arg) {
// <local state variables>
	ptask_t* tp;
	int task_id;

	tp = STATIC_CAST(ptask_t*, arg);
	task_id = ptask_get_id(tp);

	// <variables initialization and initial computation>

	ptask_start_period(tp);

	while (// <contition> ==
		true)
	{
		// <task body>

		if (ptask_deadline_miss(tp))
			printf("!");

		ptask_wait_for_period(tp);
	}

	// <optional cleanup>

	return NULL;
}

// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          USING THE LIBRARY
// -----------------------------------------------------------------------------

// Just an example of a process using this library

// NOTE: the example doesn't execute any check over the returned values. An
// actual program should always check for success of each call.


#include "ptask.h"

// ...

ptask_set_scheduler(SCHED_RR);

// This should be a static variable or in any case a variable with a lifetime
// longer than the lifetime of the relative task
ptask_t task0;

if(ptask_short(&task0, 10000l, 20, 20, 1, task_body))
{
	printf("Something wrong happened!");
	return -1;
}

// ...

ptask_join(task0);

// -----------------------------------------------------------------------------

*/

#endif
