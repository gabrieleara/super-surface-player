#include "super.h"
#include "main.h"

/*
 * Sends a quit program signal to the main thread, waiting in the loop.
 * TODO:
 */
void stop_program()
{
	//ptask_mutex_lock(&global_state.main_mutex);

	// global_state.quitting = true;
	// ptask_cond_signal(&global_state.main_cond);

	// ptask_mutex_unlock(&global_state.main_mutex);
}

/*
 * Asynchronous (TODO:) procedure called by Allegro when the close button of the
 * window is pressed.
 */
void close_button_proc()
{
	stop_program();
}

/*
 * Initializes all the program resources and Allegro modules.
 * Returns zero on success, an error code otherwise.
 */
int program_init()
{
int err;

#ifdef SUPER_DEBUG
	// Cannot debug using sudo privileges, so for debugging purposes I'll use
	// another scheduler
	err = ptask_set_scheduler(SCHED_OTHER); if(err) return err;
#else
	err = ptask_set_scheduler(SCHED_FIFO); if(err) return err;
#endif

	err = allegro_init(); if (err) return err;

	// TODO: is allegro timer needed?

	err = install_keyboard(); if (err) return err;

	err = install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL);
	if (err) return err;

	// TODO: for recording there is other stuff for sure

	// set_color_depth(COLOR_MODE);

	// err = set_gfx_mode(GFX_AUTODETECT_WINDOWED, WIN_MX, WIN_MY, 0, 0);
	// if(err) return err;

	err = install_mouse(); if(err < 0) return EPERM;

	enable_hardware_cursor();
	show_mouse(screen);

	// Initializing semaphores
	//ptask_mutex_init(&global_state.main_mutex);
	//ptask_cond_init(&global_state.main_cond);

	set_close_button_callback(close_button_proc);

	return 0;

}

/*
 * Loads all the graphic files and bitmaps needed for program graphic interface.
 * Returns zero on success, an error code otherwise.
 */
int load_resources()
{
	// TODO: load all the graphic files and bitmaps needed for program user
	// interface
	return 0;
}


/*
 * Loads into the shared data structure the background of the application.
 * TODO: Actually the static interface could be simply loaded from a bitmap
 *       file but I don't have any at the moment
 */
void init_background_interface()
{
BITMAP* static_interface;

	// static_interface = create_bitmap(WIN_MX, WIN_MY);

	// TODO: ...

	// global_state.gui.static_interface = static_interface;
}

/*
 * Initializes the graphic interface of the program.
 */
void initialize_interface()
{
	printf("Initializing interface...\r\n");

	init_background_interface();

	printf("Initializing virtual screen...\r\n");

	// TODO:

	// global_state.gui.virtual_screen = create_bitmap(WIN_MX, WIN_MY);

	// Copies the background into the virtual screen
	// blit(global_state.gui.static_interface, global_state.gui.virtual_screen,
	// 	0, 0, 0, 0, WIN_MX, WIN_MY);
}

/*
 * Initializes and starts the graphic and the user interface tasks in the program.
 * Returns zero on success, an error code otherwise.
 */
int initialize_user_tasks()
{
// int err;
//
//	printf("Starting GUI Task...\r\n");
//
//	err = start_gui_task();
//
//	if(err)
//	{
//		printf("Failed to start GUI Task!\r\n");
//		return EXIT_FAILURE;
//	}
//
//	printf("Starting UI Task...\r\n");
//
//	err = start_ui_task();
//
//	if(err)
//	{
//		printf("Failed to start UI Task!\r\n");
//
//		abort_gui_task();
//
//		return EXIT_FAILURE;
//	}

	return 0;
}

/*
 * Forever loop for the main thread, until the program termination is requested.
 * TODO: add also the functionality to START these tasks.
 */
void main_wait()
{
//	ptask_mutex_lock(&global_state.main_mutex);
//
//	while(!global_state.quitting)
//	{
//
//		while(global_state.stopping_recording == false
//			&& global_state.starting_recording == false
//			&& global_state.quitting == false)
//		{
//			ptask_cond_wait(&global_state.main_cond, &global_state.main_mutex);
//		}
//
//		if (global_state.stopping_recording
//			|| (global_state.quitting && global_state.recording))
//		{
//			global_state.recording = false;
//
//			ptask_mutex_unlock(&global_state.main_mutex);
//
//			// Wait for their termination (join)
//			//ptask_join(&tasks[MIC_TASK]);
//			//ptask_join(&tasks[FFT__TASK]);
//			//ptask_join(&tasks[ALS_TASK]);
//			//ptask_join(&tasks[PLR__TASK]);
//
//			ptask_mutex_lock(&global_state.main_mutex);
//
//			global_state.stopping_recording = false;
//		}
//		else if (global_state.starting_recording
//			&& global_state.quitting == false)
//		{
//
//			global_state.recording = true;
//
//			// Start the new tasks
//
//			global_state.starting_recording = false;
//		}
//
//	}
//
//	ptask_mutex_unlock(&global_state.main_mutex);
//
//	// Wait for termination of other tasks
//	ptask_join(&tasks[GUI_TASK]);
//	ptask_join(&tasks[UI__TASK]);
}



int main(int argc, char* argv[])
{
	printf("Initializing program...\r\n");

	if(program_init())
	{
		printf("Failed program initialization!\r\n");
		return EXIT_FAILURE;
	}

	printf("Loading recource files...\r\n");

	if(load_resources())
	{
		printf("Failed resource loading!\r\n");
		allegro_exit();
		return EXIT_FAILURE;
	}

	initialize_interface();

	if(initialize_user_tasks())
	{
		allegro_exit();
		return EXIT_FAILURE;
	}

	main_wait();

	return EXIT_SUCCESS;
}