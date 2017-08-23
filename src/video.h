#ifndef VIDEO_H
#define VIDEO_H

/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Initializes the mutex and the other required data structures.
 */
extern int video_init();


/*
 * Initializes the graphic mode by creating a new window.
 */
extern int gui_graphic_mode_init();

/*
 * Destroys the window.
 */
extern void gui_graphic_mode_exit();

/* -----------------------------------------------------------------------------
 * TASKS
 * -----------------------------------------------------------------------------
 */

extern void* gui_task(void* arg);
extern void* user_interaction_task(void* arg);


#endif