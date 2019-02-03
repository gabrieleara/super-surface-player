/**
 * @file ptask.c
 * @brief Periodic tasks utility functions
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * For actual documentation, chechout the corresponding header file, api/ptask.h.
 *
 * Inspired by the functions shown by Professor Giorgio Buttazzo during his
 * Real-Time Systems class.
 *
 */

#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "api/time_utils.h"
#include "api/ptask.h"

//-------------------------------------------------------------
// GLOBAL PRIVATE VARIABLES
//-------------------------------------------------------------

static const ptask_t _ptask_new_task;
									///< Used to initialize new struct __PTASK

static char _used_ids[PTASK_MAX];	///< Stores for each id if it is in use or not
static int _ntasks = 0;				///< Number of currently initialized ptasks

static int _last_task_id = -1;		///< Used to assign the id to the next
									///< ptask, a negative value is invalid

static int _scheduler = SCHED_OTHER;///< The scheduler that needs to be used
									///< with the ptasks created

//-------------------------------------------------------------
// LIBRARY PRIVATE UTILITY FUNCTIONS
//-------------------------------------------------------------

/**
 * Returns true if the given ptask has a valid id.
 */
static inline bool _ptask_isvalid_id(ptask_t *ptask)
{
	return ptask->id >= 0 && ptask->id < PTASK_MAX && _used_ids[ptask->id];
}

/**
 * Returns true if a new ptask can be allocated.
 */
static inline bool _ptask_canallocate()
{
	return _ntasks < PTASK_MAX;
}

/**
 * Assigns the next free id to the new task, returning the allocated id.
 */
static inline int _ptask_new_id()
{
	if(!_ptask_canallocate())
		return -1;

	do
	{
		_last_task_id = (_last_task_id+1) % PTASK_MAX;
	} while (_used_ids[_last_task_id]);

	++_ntasks;
	_used_ids[_last_task_id] = true;

	return _last_task_id;
}

/**
 * Releases a task identifier, so that it can be used again later.
 */
static inline void _ptask_free_id(ptask_t *ptask)
{
	if(_ptask_isvalid_id(ptask))
	{
		--_ntasks;
		_used_ids[ptask->id] = false;
	}
}

/**
 * Returns true if the ptask is in the state s, regardless of its id value.
 */
static inline bool _ptask_check_state(ptask_t *ptask, ptask_state_t s)
{
	return ptask->_state == s;
}

/**
 * Returns true if the task has a valid id and its state is different from
 * PS_FREE.
 */
static inline bool _ptask_isvalid(ptask_t *ptask)
{
	return _ptask_isvalid_id(ptask) &&
		!_ptask_check_state(ptask, PS_FREE);
}

/**
 * Returns true if the ptask is an empty structure that can be overwritten.
 */
static inline bool _ptask_isnew(ptask_t *ptask)
{
	return _ptask_isvalid_id(ptask) &&
		_ptask_check_state(ptask, PS_NEW);
}

/**
 * Returns true if the ptask is valid and it is in the PS_JOINABLE state.
 */
static inline bool _ptask_isjoinable(ptask_t *ptask)
{
	return _ptask_isvalid_id(ptask) &&
		_ptask_check_state(ptask, PS_JOINABLE);
}

/**
 * Returns true if the ptask is valid but its state is equal to PS_ERROR.
 */
static inline bool _ptask_iserror(ptask_t *ptask)
{
	return _ptask_isvalid_id(ptask) &&
		_ptask_check_state(ptask, PS_ERROR);
}

/**
 * Initializes the `_attr` field of the given ptask.
 * Returns a zero value on success, a non zero value otherwise.
 */
static inline int _ptask_attr_init(ptask_t *ptask)
{
struct sched_param mypar;		// structure used to set the scheduling priority
pthread_attr_t *attr_ptr = &ptask->_attr;
								// pointer to the _attr field of the ptask
int err;						// used to check for errors and return value


	if (_scheduler != SCHED_OTHER && ptask->priority == 0)
		return EINVAL;

	if (_scheduler == SCHED_OTHER && ptask->priority != 0)
		return EINVAL;

	err = pthread_attr_init(attr_ptr);
	if (err) return err;

	err = pthread_attr_setinheritsched(attr_ptr, PTHREAD_EXPLICIT_SCHED);
	if (err) return err;

	err = pthread_attr_setschedpolicy(attr_ptr, _scheduler);
	if (err) return err;

	mypar.sched_priority = ptask->priority;
	err = pthread_attr_setschedparam(attr_ptr, &mypar);
	return err;
}


//-------------------------------------------------------------
// LIBRARY PUBLIC FUNCTIONS
//-------------------------------------------------------------

int ptask_set_scheduler(int scheduler)
{
int ret = 0;	// returned value

	switch (scheduler)
	{
	case SCHED_OTHER:
	case SCHED_RR:
	case SCHED_FIFO:
		_scheduler = scheduler;
		break;
	//case SCHED_DEADLINE:
	default:
		ret = EINVAL;
		break;
	}

	return ret;
}

int ptask_init(ptask_t *ptask)
{
	if (!_ptask_canallocate())
		return EAGAIN;

	*ptask = _ptask_new_task;
	ptask->id = _ptask_new_id(); // Cannot fail since I already checked canallocate
	ptask->_state = PS_NEW;

	return 0;
}

int ptask_set_params(ptask_t *ptask, long wcet, int period, int deadline, int priority)
{
	if (!_ptask_isnew(ptask))
		return EINVAL;

	ptask->wcet = wcet;
	ptask->period = period;
	ptask->deadline = deadline;
	ptask->priority = priority;

	return 0;
}

int ptask_create(ptask_t *ptask, ptask_body_t *body)
{
int err;

	if (!_ptask_isnew(ptask))
		return EINVAL;

	err = _ptask_attr_init(ptask);

	if (err != 0)
	{
		ptask->_state = PS_ERROR;
		return err;
	}

	err = pthread_create(&ptask->_tid, &ptask->_attr, body, ptask);

	ptask->_state = (err) ? PS_ERROR : PS_JOINABLE;

	return err;
}

int ptask_destroy(ptask_t *ptask)
{
	if (!_ptask_iserror(ptask))
		return EINVAL;

	_ptask_free_id(ptask);

	*ptask = _ptask_new_task;

	return 0;

}

int ptask_set_args(ptask_t *ptask, void* args, size_t args_size)
{
	if (!_ptask_isnew(ptask))
		return EINVAL;

	if (args_size > PTASK_ARGS_SIZE)
		return EINVAL;

	if (args == NULL || args_size < 1)
		return 0;

	memcpy(ptask->args, args, args_size);

	return 0;
}

int ptask_short(ptask_t *ptask,
	long wcet, int period, int deadline, int priority, ptask_body_t *body,
	void* args, size_t args_size)
{
int err;

	err = ptask_init(ptask);
	if (err) return err;

	// I'm not checking the returned value because the following function cannot
	// fail in this point
	ptask_set_params(ptask, wcet, period, deadline, priority);

	err = ptask_set_args(ptask, args, args_size);

	if (err)
	{
		ptask->_state = PS_ERROR;
		ptask_destroy(ptask);
		return err;
	}

	err = ptask_create(ptask, body);

	if (err)
		ptask_destroy(ptask);

	return err;
}

int ptask_cancel(ptask_t *ptask)
{
	if (!_ptask_isjoinable(ptask))
		return EINVAL;

	return pthread_cancel(ptask->_tid);
}

int ptask_join(ptask_t *ptask)
{
int err;

	if (!_ptask_isjoinable(ptask))
		return EINVAL;

	err = pthread_join(ptask->_tid, NULL);

	_ptask_free_id(ptask);

	ptask->_state = PS_FREE;

	return err;
}

// The following functions should be called by the task itself

void ptask_start_period(ptask_t *ptask)
{
struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);

	time_copy(&(ptask->at), t);
	time_copy(&(ptask->dl), t);

	time_add_ms(&(ptask->at), ptask->period);
	time_add_ms(&(ptask->dl), ptask->deadline);
}

void ptask_wait_for_period(ptask_t *ptask)
{
	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &(ptask->at), NULL) != 0)
	{
	}

	time_add_ms(&(ptask->at), ptask->period);
	time_add_ms(&(ptask->dl), ptask->period);
}

int ptask_deadline_miss(ptask_t *ptask)
{
struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	if (time_cmp(now, ptask->dl) > 0) {
		ptask->dmiss++;
		return 1;
	}

	return 0;
}

//-------------------------------------------------------------
// GETTERS FOR PTASK ATTRIBUTES
//-------------------------------------------------------------

int ptask_get_id(ptask_t *ptask)
{
	return ptask->id;
}

long ptask_get_wcet(ptask_t *ptask)
{
	return ptask->wcet;
}

int ptask_get_period(ptask_t *ptask)
{
	return ptask->period;
}

int ptask_get_dealine(ptask_t *ptask)
{
	return ptask->deadline;
}

int ptask_get_priority(ptask_t *ptask)
{
	return ptask->priority;
}

int ptask_get_dmiss(ptask_t *ptask)
{
	return ptask->dmiss;
}

//-------------------------------------------------------------
// MUTEXES AND CONDITION VARIABLES
//-------------------------------------------------------------

static pthread_mutexattr_t _matt;	///< Attributes used to initialize any
									///< mutex allocated in the system
static pthread_once_t _once_mutex_attr;
									///< Handles whether the _matt variable is
									///< already initialized or not

/**
 * Executed once per program on the first mutex init, it initializes the mutex
 * attributes static global variable.
 */
static void _ptask_init_mutex_attr()
{
	pthread_mutexattr_init(&_matt);

	// NOTICE: Change to priority ceiling if needed
	pthread_mutexattr_setprotocol(&_matt, PTHREAD_PRIO_INHERIT);
}

int ptask_mutex_init(ptask_mutex_t *mux_p)
{
	pthread_once(&_once_mutex_attr, _ptask_init_mutex_attr);

	return pthread_mutex_init(mux_p, &_matt);
}

int ptask_mutex_lock(ptask_mutex_t *mux_p)
{
	return pthread_mutex_lock(mux_p);
}

int ptask_mutex_unlock(ptask_mutex_t *mux_p)
{
	return pthread_mutex_unlock(mux_p);
}

int ptask_cond_init(ptask_cond_t *cond_p)
{
	return pthread_cond_init(cond_p, NULL);
}

int ptask_cond_wait(ptask_cond_t *cond_p, ptask_mutex_t *mux_p)
{
	return pthread_cond_wait(cond_p, mux_p);
}

int ptask_cond_signal(ptask_cond_t *cond_p)
{
	return pthread_cond_signal(cond_p);
}

int ptask_cond_broadcast(ptask_cond_t *cond_p)
{
	return pthread_cond_broadcast(cond_p);
}


//-------------------------------------------------------------
// CYCLIC ASYNCHRONOUS BUFFERS
//-------------------------------------------------------------

static const struct __PTASK_CAB _ptask_new_cab;
									///< Used to initialize each struct
									///< __PTASK_CAB to default values

static int _next_cab_id = 1;		///< Used to assign the id to the next new
									///< cab, a zero or negative value is
									///< invalid

// NOTICE: no need to keep trace of used ids since a cab cannot be destroyed.
// Instead, cabs should be reused and reinitialized (using the reset function).
// This implementation choice is due to reliability and integrty reasons united
// with the will to write the simplest library possibile.
// In order to destroy safely a cab, a trace of all tasks that are currently
// using the cab should be kept, forbidding further acquisitions of buffers
// until all the currently used buffers have been freed. Only when no buffers
// inside the cab are used anymore the cab could be destroyed.

/// Returns true if a new cab structure can be allocated
static inline int _ptask_cab_canallocate()
{
	return _next_cab_id <= PTASK_CAB_MAX;
}

/// Allocates a new cab id
static inline int _ptask_cab_next_id()
{
	return _next_cab_id++;
}

int ptask_cab_init(ptask_cab_t *ptask_cab, int n, int size, void *buffers[])
{
int i;

	if(n > PTASK_CAB_MAX_SIZE)
		return EINVAL;

	if (!_ptask_cab_canallocate())
		return EINVAL;

	*ptask_cab = _ptask_new_cab;

	ptask_cab->id = _ptask_cab_next_id();
	ptask_cab->num_buffers = n;
	ptask_cab->size_buffers = size;
	ptask_cab->last_index = -1;
	// timestamp remains uninitialized

	for(i = 0; i < n; ++i)
		ptask_cab->buffers[i] = buffers[i];

	ptask_mutex_init(&ptask_cab->_mux);

	return 0;
}

int ptask_cab_reset(ptask_cab_t *ptask_cab)
{
	ptask_mutex_lock(&ptask_cab->_mux);

	ptask_cab->last_index = -1;

	ptask_mutex_unlock(&ptask_cab->_mux);

	return 0;
}

// NOTICE: the following functions assume that the cab is used correctly, hence
// only n-1 tasks can use the cab.

int ptask_cab_reserve(ptask_cab_t *ptask_cab, void* buffer[], ptask_cab_id_t *b_id)
{
int i = 0;

	ptask_mutex_lock(&ptask_cab->_mux);

	while (ptask_cab->busy[i] || i == ptask_cab->last_index)
		++i;

	++ptask_cab->busy[i];

	ptask_mutex_unlock(&ptask_cab->_mux);

	*buffer = ptask_cab->buffers[i];
	*b_id = i;

	return 0;
}

int ptask_cab_putmes(ptask_cab_t *ptask_cab, ptask_cab_id_t b_id)
{
int err = 0;

	ptask_mutex_lock(&ptask_cab->_mux);

	if(ptask_cab->busy[b_id] != 1 || ptask_cab->last_index == b_id)
		err = EINVAL;
	else
	{
		ptask_cab->busy[b_id] = 0;
		ptask_cab->last_index = b_id;
		clock_gettime(CLOCK_MONOTONIC, &ptask_cab->timestamp);
	}

	ptask_mutex_unlock(&ptask_cab->_mux);

	return err;
}

int ptask_cab_getmes(ptask_cab_t *ptask_cab, const void* buffer[],
	ptask_cab_id_t *b_id, struct timespec *timestamp)
{
int err = 0;

	ptask_mutex_lock(&ptask_cab->_mux);

	if(ptask_cab->last_index < 0)
		err = EAGAIN;
	else
	{
		*b_id = ptask_cab->last_index;
		if (timestamp != NULL)
			*timestamp	= ptask_cab->timestamp;
		++ptask_cab->busy[*b_id];
	}

	ptask_mutex_unlock(&ptask_cab->_mux);

	if(!err)
		*buffer = ptask_cab->buffers[*b_id];

	return err;
}

int ptask_cab_unget(ptask_cab_t *ptask_cab, ptask_cab_id_t b_id)
{
int err = 0;

	ptask_mutex_lock(&ptask_cab->_mux);

	if(ptask_cab->busy[b_id] < 1)
		err = EINVAL;
	else
		--ptask_cab->busy[b_id];

	ptask_mutex_unlock(&ptask_cab->_mux);

	return err;
}
