/**
 * @file video.h
 * @brief Video-related public functions and data types
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * Since the video module is responsible for everything printed on the screen
 * and to perform interaction with user commands by querying the modules, it is
 * mostly self-contained.
 * That's why only these few functions are exposed as public.
 *
 */


#ifndef VIDEO_H
#define VIDEO_H

// -----------------------------------------------------------------------------
//                             PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * Initializes the mutex and the other required data structures.
 */
extern int video_init();

// -----------------------------------------------------------------------------
//                                  TASKS
// -----------------------------------------------------------------------------

/// The body of the gui task
extern void* gui_task(void* arg);

/// The body of the user interaction task
extern void* user_interaction_task(void* arg);

#endif
