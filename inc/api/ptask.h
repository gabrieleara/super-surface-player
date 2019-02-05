/**
 * @file ptask.h
 * @brief Periodic tasks utility functions
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * Inspired by the functions shown by Professor Giorgio Buttazzo during his
 * Real-Time Systems class.
 *
 * @section ptask_api PTask API
 * @subsection ptask_notice Notice
 * Functions operating on ptask_t structures (with very few exceptions)
 * are unsafe from a concurrency point of view. They are supposed to be used
 * from a designated thread that handles the behavior of the whole system,
 * usually main thread.
 * Also, such functions often do not check their arguments and treat them almost
 * always as valid (i.e. no checks are executed over pointers passed by
 * argument). If the library is used as illustrated below, no problems can
 * arise.
 *
 * @subsection using_ptask Using the Library
 * Following is an example of usage of the library. First we will show an
 * example of a simple PTask body and then an example of its initialization from
 * the main thread.
 *
 * \code

// -----------------------------------------------------------------------------
//                             TASK EXAMPLE
// -----------------------------------------------------------------------------

// Structure of a task, its argument is the pointer to the previously defined
// descriptor structure

void* task_body(void* arg)
{
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

 * \endcode
 *
 * \code

// -----------------------------------------------------------------------------
//                          USING THE LIBRARY
// -----------------------------------------------------------------------------

// Just an example of a process using this library

// NOTICE: the example doesn't execute any check over the returned values. An
// actual program should always check for success of each call.


#include "ptask.h"

// ...

ptask_set_scheduler(SCHED_RR);

// This should be a static variable or in any case a variable with a lifetime
// longer than the lifetime of the relative task
ptask_t task0;

if(ptask_short(&task0, 10000l, 20, 20, 1, task_body, NULL, 0))
{
	printf("Something wrong happened!");
	return -1;
}

// ...

ptask_join(task0);

 * \endcode
 *
 * @section ptask_mutex_cond PTask Mutexes and Condition Variables
 * In current phase of development, PTask Mutexes and Condition Variables are
 * simply an alias for corresponsing PThread structures. However, this is not
 * guaranteed in the future and users should stick to the functions provided by
 * this library when using them.
 *
 * While the implementation of said functions may change, semantically the
 * functions are the same as the corresponding PThread ones and thus refer to
 * them for further details.
 *
 * Arguments used in pthread library that are not shown in these functions
 * interface are handled by this library itself.
 *
 */

#ifndef PTASKS_H
#define PTASKS_H

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

/// The maximum number of tasks that can be allocated at any time
#define PTASK_MAX	(50)

/**
 * The possible states in which a task can be:
 * - PS_FREE: the task has not been associated with any actual job
 * - PS_NEW: the task has been reserved for a job that hasn't started yet
 * - PS_JOINABLE: the task has been started, thus it it either waiting to be
 * executed by the scheduler, running or terminated and it can be joined.
 * - PS_ERROR: for an erroneous task state.
 */
typedef enum __PTASK_ENUM {
	PS_ERROR = -1,
	PS_FREE = 0,
	PS_NEW,
	//PS_ACTIVE,
	PS_JOINABLE
} ptask_state_t;

/// The maximum number of bytes that can be given as argument to a ptask
#define PTASK_ARGS_SIZE	(32)

/**
 * The structure representing a task
 */
typedef struct __PTASK_STRUCT
{
	int id;				///< Identificator of a ptask
	long wcet;			///< Worst case execution time (in us): 0 means unknown
	int period;			///< in milliseconds
	int deadline;		///< Relative to activation time (in ms)
	int priority;		///< Value between [0,99], standard should be in [0,32]
	int dmiss;			///< Number of occurred deadline misses
	struct timespec at;	///< Next activation time
	struct timespec dl;	///< Next absolute deadline

	ptask_state_t _state;///< State of the ptask, see ptask_state_t

	pthread_t _tid;		///< Pthread id of the task
	pthread_attr_t _attr;///< Pthread params of the task

	char args[PTASK_ARGS_SIZE];
						///< Extra arguments that may be given to the task,
						///< up to PTASK_ARGS_SIZE bytes
} ptask_t;

/// Alias of phtread_mutex_t
typedef pthread_mutex_t ptask_mutex_t;
/// Alias of phtread_cond_t
typedef pthread_cond_t ptask_cond_t;

/// The maximum number of cab structures that can be allocated in a process
#define PTASK_CAB_MAX		(50)

/// The maximum number of buffers within a cab structure
#define PTASK_CAB_MAX_SIZE	(32)

/**
 * The structure representing a CAB
 */
typedef struct __PTASK_CAB
{
	int	id;							///< identificator of the cab
	int num_buffers;				///< number of buffers in the cab
	int size_buffers;				///< size of each buffer in the cab in bytes (all equal)

	void *buffers[PTASK_CAB_MAX_SIZE];
									///< pointers to the actual buffers

	int busy[PTASK_CAB_MAX_SIZE];	///< number of tasks using each buffer
	int last_index;					///< pointer to the current last data buffer
	struct timespec timestamp;		///< time at which the most recent buffer has been inserted

	ptask_mutex_t _mux;				///< mutex semaphore used to access the cab structure
} ptask_cab_t;

/**
 * This alias is useful when defining pointers to functions that can be
 * tasks bodies.
 */
typedef void* (ptask_body_t) (void*);

/**
 * The type of a CAB identifier
 */
typedef int ptask_cab_id_t;

//-------------------------------------------------------------
// LIBRARY PUBLIC FUNCTIONS
//-------------------------------------------------------------

/**
 * @name Functions that operate on ptask_t objects
 */
//@{

/**
 * Sets the scheduler to the given value. Accepted values are
 * - SCHED_OTHER
 * - SCHED_RR
 * - SCHED_FIFO
 * - SCHED_DEADLINE (currently not supported, returns EINVAL)
 *
 * All other values will make this function fail, returning EINVAL. On success
 * this function returns 0.
 *
 * NOTICE: Scheduler should only be set once per process execution, before all
 * other threads than the main thread have been started.
 */
extern int ptask_set_scheduler(int scheduler);

/**
 * Initializes a new ptask; if no new ptasks can be initialized it returns
 * EAGAIN.
 *
 * Returns zero otherwise.
 *
 * NOTICE: on success this function overwrites any value it is inside the given
 * ptask_t element. Do not use an already-initialized ptask object with this
 * function unless it has been previously destroyed/joined. The ptask_short
 * function already destroys tasks that failed initialization.
 */
extern int ptask_init(ptask_t *ptask);

/**
 * Sets each of the indicated parameters of a given ptask.
 * Returns 0 on success, a non zero value otherwise.
 *
 * NOTICE: this function can be called only before starting the ptask.
 */
extern int ptask_set_params(ptask_t *ptask, long wcet, int period, int deadline,
	int priority);

/**
 * Copies the given arguments into the ptask_t structure, so that the task can
 * later retrieve them.
 * The parameter args must be a pointer to a contiguous memory area of at
 * least args_size bytes.
 * Returns 0 on success, a non zero value otherwise.
 *
 * NOTICE: this function can be called only before starting the ptask.
 */
extern int ptask_set_args(ptask_t *ptask, void *args, size_t args_size);

/**
 * Creates a new (previously initialized) ptask and starts its execution with
 * the given body function.
 * Returns 0 on success, a non zero value otherwise.
 */
extern int ptask_create(ptask_t *ptask, ptask_body_t *body);

/**
 * Destroys a previously initialied ptask but only if the ptask_create
 * function previously failed.
 * Returns 0 on success, a non zero value otherwise.
 */
extern int ptask_destroy(ptask_t *ptask);

/**
 * Shorthand for the creation of a task.
 * Returns 0 on success, a non zero value otherwise.
 */
extern int ptask_short(ptask_t *ptask,
	long wcet, int period, int deadline, int priority, ptask_body_t *body,
	void *args, size_t args_size);

/**
 * Cancels a previously started ptask, if it is still running.
 * Returns 0 on success, a non zero value otherwise.
 *
 * NOTICE: this function creates only a cancelation request, thus is not
 * blocking. To be sure that a ptask is actually terminated call ptask_join.
 *
 * NOTICE: this function may lead global values to inconsistency and should not
 * be used unless the program has to be forcefully terminated. Please a custom
 * termination condition inside your tasks to terminate them gracefully.
 */
extern int ptask_cancel(ptask_t *ptask);

/**
 * Waits until a ptask has terminated its execution.
 * Returns zero on success, a non zero value otherwise.
 */
extern int ptask_join(ptask_t *ptask);

// The following functions should be called by the task itself

/**
 * Reads the current time and computes the next activation time and the
 * absolute deadline of the task.
 *
 * This function shall be called by the task itself.
 */
extern void ptask_start_period(ptask_t *ptask);

/**
 * Suspends the calling task until the next activation and, when awaken,
 * updates activation time and deadline.
 *
 * This function shall be called by the task itself.
 *
 */
extern void ptask_wait_for_period(ptask_t *ptask);

/**
 * If the task is still in execution after its deadline, it increments the
 * value of dmiss and returns a non zero value, otherwise returns zero.
 *
 * This function shall be called by the task itself.
 */
extern int ptask_deadline_miss(ptask_t *ptask);

//@}

//-------------------------------------------------------------
// GETTERS FOR PTASK ATTRIBUTES
//-------------------------------------------------------------

/**
 * @name Getters for ptask_t attributes
 */
//@{

/// Returns the task id
extern int ptask_get_id(ptask_t *ptask);
/// Returns the task WCET
extern long ptask_get_wcet(ptask_t *ptask);
/// Returns the task period
extern int ptask_get_period(ptask_t *ptask);
/// Returns the task deadline
extern int ptask_get_dealine(ptask_t *ptask);
/// Returns the task priority
extern int ptask_get_priority(ptask_t *ptask);
/// Returns the number of deadline misses experienced by the task
extern int ptask_get_dmiss(ptask_t *ptask);

//@}

//-------------------------------------------------------------
// MUTEXES AND CONDITION VARIABLES
//-------------------------------------------------------------

/**
 * @name Mutex and Condition Variables
 */
//@{

/// Initializes a new ptask_mutex_t
extern int ptask_mutex_init(ptask_mutex_t *mux_p);
/// Locks a ptask_mutex_t
extern int ptask_mutex_lock(ptask_mutex_t *mux_p);
/// Unlocks a ptask_mutex_t
extern int ptask_mutex_unlock(ptask_mutex_t *mux_p);
/// Initializes a new ptask_cond_t
extern int ptask_cond_init(ptask_cond_t *cond_p);
/// Waits for the given ptask_cond_t, releasing the ptask_mutex_t and
/// re-acquiring it later
extern int ptask_cond_wait(ptask_cond_t *cond_p, ptask_mutex_t *mux_p);
/// Signals one of the tasks waiting for the given ptask_cond_t
extern int ptask_cond_signal(ptask_cond_t *cond_p);
/// Broadcasts a signal to all tasks waiting for the given ptask_cond_t
extern int ptask_cond_broadcast(ptask_cond_t *cond_p);

//@}

//-------------------------------------------------------------
// CYCLIC ASYNCHRONOUS BUFFERS
//-------------------------------------------------------------

/**
 * @name Cyclic Asynchronous Buffers
 */
//@{

/**
 * Initiaizes a new cab, with the given n buffers, each with the given size.
 * Returns zero on success, a non zero value otherwise. Notice that if the
 * maximum number of CABs has already been assigned this function will return
 * EINVAL.
 */
extern int ptask_cab_init(ptask_cab_t *ptask_cab, int n, int size,
	void *buffers[]);

/**
 * Resets the cab last value to no value, as it was just been initialized.
 * Any task using a cab buffer can continue using it without any problem.
 *
 * Returns zero on success, a non zero value otherwise.
 *
 * NOTICE: Make sure that all the readers released their reading buffers before a
 * new value is inserted into the cab.
 */
extern int ptask_cab_reset(ptask_cab_t *ptask_cab);


// NOTICE: the following functions assume that the cab is used correctly, hence
// only n-1 tasks can use the cab.

/**
 * This function reserves a cab for writing purposes.
 * On success, the buffer pointer given by argument will contain a pointer to
 * the reserved buffer and the b_id pointer will contain the id of the said
 * buffer. This id will be used later to commit any changes applied to the
 * buffer using ptask_cab_putmes.
 *
 * It returns zero on success, a non zero value otherwise.
 *
 * NOTICE: Attempting to reserve a buffer using a non already initialized cab
 * results in undefined behavior.
 */
extern int ptask_cab_reserve(ptask_cab_t *ptask_cab, void* buffer[],
	ptask_cab_id_t *b_id);

/**
 * This function commits the value written in a cab buffer as the new last
 * message put in the cab. It MUST be called after a ptask_cab_reserve call
 * using the b_id value obtained by it and ONLY ONCE.
 *
 * On success, the given buffer is set as the one containing of the last message
 * in the cab.
 *
 * It returns zero on success, a non zero value otherwise.
 *
 * NOTICE: Attempting to put a message inside a non already initialized cab
 * results in undefined behavior.
 *
 * NOTICE: Attempting to put a message inside a non previously reserved buffer
 * may lead to inconsistency if the buffer is actually being used by only one
 * ptask in readig mode.
 */
extern int ptask_cab_putmes(ptask_cab_t *ptask_cab, ptask_cab_id_t b_id);


/**
 * This function reserves a cab for reading purposes.
 * On success, the buffer pointer given by argument will contain a pointer to
 * the reserved buffer and the b_id pointer will contain the id of the said
 * buffer. This id will be used later to release the obtained buffer using
 * ptask_cab_unget.
 *
 * Last argument is an OUT argument that contains the timestamp at which said
 * buffer has been produced. Accepts also NULL if non interested.
 *
 * It returns zero on success, a non zero value otherwise. In particular,
 * EAGAIN is returned if no value has been put inside the cab or the cab has
 * been resetted using ptask_cab_reset.
 *
 * NOTICE: Attempting to reserve a buffer using a non already initialized cab
 * results in undefined behavior.
 */
extern int ptask_cab_getmes(ptask_cab_t *ptask_cab, const void* buffer[],
	ptask_cab_id_t *b_id, struct timespec *timestamp);

/**
 * This function releases a buffer acquired for reading purposes using the
 * ptask_cab_getmes. It MUST be called after a ptask_cab_getmes call
 * using the b_id value obtained by it and ONLY ONCE.
 *
 * It returns zero on success, a non zero value otherwise.
 *
 * NOTICE: Attempting to release a buffer within a non already initialized cab
 * results in undefined behavior.
 *
 * NOTICE: This can be used also to cancel a reservation request, thus this
 * function can be called once also after a successful call to
 * ptask_cab_reserve.
 */
extern int ptask_cab_unget(ptask_cab_t *ptask_cab, ptask_cab_id_t b_id);

//@}

#endif
