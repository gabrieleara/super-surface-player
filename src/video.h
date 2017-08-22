#ifndef VIDEO_H
#define VIDEO_H

/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Initializes the graphic mode, by creating a new window and starting recording.
 */
extern int gui_graphic_mode_init();

/*
 * Destroys the window and removes all mouse and keyboard handlers.
 */
extern void gui_graphic_mode_exit();

/* -----------------------------------------------------------------------------
 * TASKS
 * -----------------------------------------------------------------------------
 */

extern void* gui_task(void* arg);


#endif