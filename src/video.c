/**
 * @file video.c
 * @brief Video-related functions and data types
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * For public functions, documentation can be found in corresponding header file:
 * video.h.
 *
 */

// Standard libraries
#include <stdbool.h>
#include <stdio.h>

#include <assert.h>			// Used in debug

// Linked libraries
#include <allegro.h>

// Custom libraries
#include "api/std_emu.h"
#include "api/time_utils.h"
#include "api/ptask.h"

// Other modules
#include "constants.h"
#include "main.h"
#include "audio.h"
#include "video.h"

// -----------------------------------------------------------------------------
//                           PRIVATE CONSTANTS
// -----------------------------------------------------------------------------

#define BITMAP_RES_FOLDER		"res/"	///< Folder in which bitmaps are placed
#define BITMAP_BACKGROUND_PATH	BITMAP_RES_FOLDER "background.bmp"
										///< Background bitmap relative path
#define BITMAP_S_ELEMENT_PATH	BITMAP_RES_FOLDER "element.bmp"
										///< Sample-based audio entry bitmap relative path
#define BITMAP_M_ELEMENT_PATH	BITMAP_RES_FOLDER "element_midi.bmp"
										///< MIDI-based audio entry bitmap relative path

#define STR_VOLUME				"Volume"			///< Volume control string
#define STR_PANNING				"Panning"			///< Panning control string
#define STR_FREQUENCY			"Base Frequency"	///< Frequency adjustment control string


// -----------------------------------------------------------------------------
//                           PRIVATE DATA TYPES
// -----------------------------------------------------------------------------

/**
 * It contains all the static elements of the gui (backgrounds), reused when
 * multiple elements of the same time are drawn.
 */
typedef struct __GUI_STATIC_STRUCT
{
	BITMAP* background;		///< Static background of the interface

	BITMAP* element_sample;	///< Static background of an element of the
							///< interface representing an opened sample

	BITMAP* element_midi;	///< Static background of an element of the
							///< interface representing an opened midi

} gui_static_t;

/// Global state of the module
typedef struct __GUI_STRUCT
{
	// NOTE: When modifying the main_state_t data structure, consider
	// which state should be adopted as default state.

	BITMAP*		virtual_screen;	///< Virtual screen, used to handle all gui
								///< operations, it is copied in the actual screen
								///< when refreshed

	gui_static_t static_screen;	///< Contains all the gui bitmaps

	bool		initialized;	///< Tells if this interface has been initialized
								///< i.e.\ all bitmaps are loaded and so on

	bool		mouse_initialized;
								///< Tells if the mouse handler has been
								///< initialized
	bool		mouse_shown;	///< Tells if the show_mouse function has been
								///< called on the current screen

	ptask_mutex_t mutex;		///< This mutex is used to protect the access
								///< to mouse flags
} gui_state_t;

/**
 * The complete list of all buttons on the UI.
 * Notice that the buttons are replicated for each audio file entry, thus the
 * number of buttons on the screen may vary depending how many audio files are
 * open.
 */
typedef enum __BUTTON_ID_ENUM
{
	BUTTON_INVALID = -1,	///< A non valid button, used when a button id is returned
	BUTTON_PLAY,			///< Play button
	BUTTON_VOL_DOWN,		///< Volume down
	BUTTON_VOL_UP,			///< Volume up
	BUTTON_PAN_DOWN,		///< Panning down
	BUTTON_PAN_UP,			///< Panning up
	BUTTON_FRQ_DOWN,		///< Frequency adjustment down
	BUTTON_FRQ_UP,			///< Frequency adjustment up
} button_id_t;

// -----------------------------------------------------------------------------
//                           GLOBAL VARIABLES
// -----------------------------------------------------------------------------

/// The global state of the gui module
static gui_state_t gui_state =
{
	.initialized		= false,
	.mouse_initialized	= false,
	.mouse_shown		= false,
};

// -----------------------------------------------------------------------------
//                           PRIVATE FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * @name Private functions
 */
//@{

/**
 * If not previously initialized, loads all the interface static members.
 * It can be called multiple times, but the body will be only executed the first
 * time.
 *
 * NOTICE: this function assumes to be called from a single-thread, it is not
 * safe from a concurrency point of view.
 */
static inline int static_interface_init()
{
BITMAP* 		bitmap_ptr;		// Temporary pointer to a BITMAP
gui_static_t	static_screen;	// Holds all the static interface elements

	if(gui_state.initialized)
		return 0;

	bitmap_ptr = load_bitmap(BITMAP_BACKGROUND_PATH, NULL);
	if (bitmap_ptr == NULL) return EINVAL;

	static_screen.background = bitmap_ptr;

	bitmap_ptr = load_bitmap(BITMAP_S_ELEMENT_PATH, NULL);
	if (bitmap_ptr == NULL) return EINVAL;

	// Adds texts statically to the base element bitmap, so that the only font
	// used in the graphic interface is the Allegro one
	textout_ex(bitmap_ptr, font, STR_VOLUME,
		SIDE_ELEM_VOL_LABEL_X, SIDE_ELEM_VOL_LABEL_Y,
		COLOR_TEXT_PRIM, COLOR_BKG);

	textout_ex(bitmap_ptr, font, STR_PANNING,
		SIDE_ELEM_PAN_LABEL_X, SIDE_ELEM_PAN_LABEL_Y,
		COLOR_TEXT_PRIM, COLOR_BKG);

	textout_ex(bitmap_ptr, font, STR_FREQUENCY,
		SIDE_ELEM_FRQ_LABEL_X, SIDE_ELEM_FRQ_LABEL_Y,
		COLOR_TEXT_PRIM, COLOR_BKG);

	static_screen.element_sample = bitmap_ptr;

	bitmap_ptr = load_bitmap(BITMAP_M_ELEMENT_PATH, NULL);
	if (bitmap_ptr == NULL) return EINVAL;

	static_screen.element_midi	= bitmap_ptr;

	// Save static data and create virtual screen bitmap
	gui_state.static_screen		= static_screen;
	gui_state.virtual_screen	= create_bitmap(WIN_MX, WIN_MY);
	gui_state.initialized		= true;

	return 0;
}

/**
 * Copies the background onto the virtual screen.
 */
static inline void draw_background()
{
	blit(gui_state.static_screen.background, gui_state.virtual_screen,
		0, 0, 0, 0, WIN_MX, WIN_MY);
}

/**
 * Draws the given index element, assuming that it is an audio sample element.
 */
static inline void draw_side_element_sample(int index)
{
int		posx, posy;	// Starting point where to draw the given element
char	buffer[4];	// Buffer string used to print on the screen
int		value;		// Value where to store

	posx = SIDE_X;
	posy = SIDE_Y + index * SIDE_ELEM_MY;

	blit(
		gui_state.static_screen.element_sample,
		gui_state.virtual_screen,
		0, 0,
		posx, posy,
		SIDE_ELEM_MX, SIDE_ELEM_MY);

	textout_ex(
		gui_state.virtual_screen,
		font,
		audio_get_filename(index),
		SIDE_ELEM_NAME_X, posy+SIDE_ELEM_NAME_Y,
		COLOR_TEXT_PRIM, COLOR_BKG);

	value = audio_get_volume(index);

	sprintf(buffer, "%d", value);
	textout_ex(
		gui_state.virtual_screen,
		font,
		buffer,
		SIDE_ELEM_VOL_X, posy+SIDE_ELEM_VAL_Y,
		COLOR_TEXT_PRIM, COLOR_WHITE);

	value = audio_get_panning(index);

	sprintf(buffer, "%d", value);
	textout_ex(
		gui_state.virtual_screen,
		font,
		buffer,
		SIDE_ELEM_PAN_X, posy+SIDE_ELEM_VAL_Y,
		COLOR_TEXT_PRIM, COLOR_WHITE);

	value = audio_get_frequency(index);

	sprintf(buffer, "%d", value);
	textout_ex(gui_state.virtual_screen, font, buffer,
		SIDE_ELEM_FRQ_X, posy+SIDE_ELEM_VAL_Y,
		COLOR_TEXT_PRIM, COLOR_WHITE);
}

/**
 * Draws the given index element, assuming that it is a midi element.
 */
static inline void draw_side_element_midi(int index)
{
int posx, posy;	// Starting point where to draw the given element
	posx = SIDE_X;
	posy = SIDE_Y + index * SIDE_ELEM_MY;

	blit(
		gui_state.static_screen.element_midi,
		gui_state.virtual_screen,
		0, 0,
		posx, posy,
		SIDE_ELEM_MX, SIDE_ELEM_MY);

	textout_ex(
		gui_state.virtual_screen,
		font,
		audio_get_filename(index),
		SIDE_ELEM_NAME_X, posy+SIDE_ELEM_NAME_Y,
		COLOR_TEXT_PRIM, COLOR_BKG);
}


/**
 * Draws the given index element.
 */
static inline void draw_side_element(int index)
{
	switch (audio_get_type(index))
	{
	case AUDIO_TYPE_SAMPLE:
		draw_side_element_sample(index);
		break;

	case AUDIO_TYPE_MIDI:
		draw_side_element_midi(index);
		break;

	default:
		assert(false);
	}
}

/**
 * Overwrites the whole sidebar on the virtual screen.
 */
static inline void draw_sidebar()
{
int num, i;

	num = audio_get_num_files();

	for (i = 0; i < num; ++i)
	{
		draw_side_element(i);
	}
}

/**
 * Draws the FFT of the (last) recorded audio on the screen.
 */
static inline void draw_fft()
{
double *buffer;
int rate;
int frames;

	// TODO: To implement this function, try to find again the code that you
	// wrote almost two years year ago!
}

/**
 * Computes the energy of the last known audio input sample.
 */
static inline int compute_last_amplitude()
{
int		buffer_dim;		// dimension of the buffer
int		buffer_index;	// index of the buffer, to release it later
int		i;
short*	buffer;			// the buffer containing the last audio sample
double	amplitude = 0;	// the computed average energy

	// Get last audio input buffer
	buffer_dim = audio_get_last_record(&buffer, &buffer_index);

	if (buffer_dim > 0)
	{
		// First calculate the sum of squares, then average
		for (i = 0; i < buffer_dim; ++i)
		{
			amplitude +=
				STATIC_CAST(double, buffer[i]) * STATIC_CAST(double, buffer[i]);
		}

		// Free the buffer
		audio_free_last_record(buffer_index);

		// Compute average, sqrt is not necessary because what we plot hasn't
		// actually got any specific scale
		amplitude /= STATIC_CAST(double, buffer_dim);
	}

	return (int) amplitude;
}

/**
 * Converts the given amplitude (energy) value to the height of its
 * corresponding bar in the plot, applying a saturation if the value is too big.
 */
static inline int amplitude_to_height(int ampl)
{
double precise_value;	// used to perform a reliable computation with big numbers
int res;

	// Check beforehand saturation
	if (ampl > TIME_MAX_AMPLITUDE)
		return TIME_MAX_HEIGHT;

	// The height is lineraly proportional to the amplitude, with a maximum value
	precise_value = STATIC_CAST(double,TIME_MAX_HEIGHT) * STATIC_CAST(double,ampl);
	precise_value /= STATIC_CAST(double,TIME_MAX_AMPLITUDE);

	res = STATIC_CAST(int,precise_value);

	// Check again saturation
	if (res > TIME_MAX_HEIGHT)
		res = TIME_MAX_HEIGHT;

	return res;
}

/**
 * Draws the energy history of the input signal on the screen.
 */
static inline void draw_amplitude()
{
// TODO: Consider changing the organization of this function and to adopt more
// global variables/constants

// TODO: documentation
static BITMAP *amplitude_bitmap = NULL;

int amplitude;

	if (amplitude_bitmap == NULL)
	{
		// On first execution, a new bitmap is created, with the same content of
		// the virtual screen in the area of the time plot
		amplitude_bitmap = create_bitmap(TIME_PLOT_WIDTH, TIME_PLOT_HEIGHT);
		blit(gui_state.virtual_screen, amplitude_bitmap,
			TIME_PLOT_X,
			TIME_PLOT_Y,
			0,
			0,
			TIME_PLOT_WIDTH,
			TIME_PLOT_HEIGHT);
	}

	// Copy back the plot on the virtual screen from the history bitmap, which
	// is already shifted by TIME_SPEED amount.
	// Instead of keeping track all past amplitude values, the bitmap keeps
	// track of the past values, each time shifting the screen by TIME_SPEED
	// pixels.
	blit(amplitude_bitmap, gui_state.virtual_screen,
		 TIME_SPEED,
		 0,
		 TIME_PLOT_X,
		 TIME_PLOT_Y,
		 TIME_PLOT_WIDTH - TIME_SPEED,
		 TIME_PLOT_HEIGHT);

	// Convert the last computed amplitude to the actual height in pixels
	amplitude = amplitude_to_height(compute_last_amplitude());

	// Plot the last amplitude on the virtual screen, the width of the column is
	// TIME_SPEED pixels and the height depends linearly on the amplitude, as
	// computed above.
	rectfill(gui_state.virtual_screen,
			 TIME_PLOT_MX - TIME_SPEED,
			 TIME_PLOT_MY - amplitude - 1,
			 TIME_PLOT_MX - 1,
			 TIME_PLOT_MY - 1,
			 COLOR_ACCENT);

	// Shift the plot on the virtual screen for next execution.
	// NOTICE: we can't use the virtual screen itself because it's cleared at
	// each execution.
	// TODO: check whether this statement is true.
	blit(gui_state.virtual_screen, amplitude_bitmap,
		 TIME_PLOT_X,
		 TIME_PLOT_Y,
		 0,
		 0,
		 TIME_PLOT_WIDTH,
		 TIME_PLOT_HEIGHT);
}

/**
 * Calls Allegro show_mouse(screen) if the mouse module has been initialized,
 * but only once at first run each time the window is created.
 *
 * TODO: rename this one.
 */
static inline void my_show_mouse()
{
bool result;

	ptask_mutex_lock(&gui_state.mutex);

	result = gui_state.mouse_initialized && !gui_state.mouse_shown;

	if (result)
		gui_state.mouse_shown = true;

	ptask_mutex_unlock(&gui_state.mutex);

	if (result)
		show_mouse(screen);
}

/**
 * Refreshes the content of the Allegro window.
 */
static inline void screen_refresh()
{
	draw_background();

	draw_sidebar();

	draw_fft();

	draw_amplitude();

	// Previous operations all work on the virtual screen, at the very end we
	// copy the virtual screen on the actual screen variable
	blit(gui_state.virtual_screen, screen, 0, 0, 0, 0, WIN_MX, WIN_MY);

	my_show_mouse();
}

/**
 * Asynchronous procedure called by Allegro when the close button of the
 * window is pressed.
 * Called only when the graphic mode is active.
 */
static inline void close_button_proc()
{
	// Request the main module to terminate current session
	main_terminate_tasks();
}

// TODO: move somewhere else
#define MAX_KEY_COMMANDS	256

/**
 * Handles actions that are triggered by numeric keys.
 * Each key from 1 to 0 triggers the execution of the corresponding audio file
 * (where 0 represents the 10th audio file).
 */
static inline void handle_num_key(int num)
{
	num = num == 0 ? 9 : num-1;

	if (num < audio_get_num_files())
	{
		audio_play_file(num);
	}
}

/**
 * Checks whether the user has given commands to the program while in graphic
 * mode.
 * Commands are specified by single key press.
 */
static inline void handle_keyboard_inputs()
{
int count;
int key;
int scancode;

	for(count = 0; keypressed() && count < MAX_KEY_COMMANDS; count++)
	{
		key = readkey();

		scancode = key >> 8;

		if (scancode >= KEY_0 && scancode <= KEY_9)
			handle_num_key(scancode - KEY_0);
		else
		{
			switch(key >> 8)
			{
			case KEY_Q:
				// Request main module to terminate the current session
				main_terminate_tasks();
				break;
			// IDEA: Implement global volume controls, Up/Down.
			default:
				// Do nothing
				break;
			}
		}
	}

	if (keypressed() && count >= MAX_KEY_COMMANDS)
	{
		printf("UI_TASK: Too many keyboard commands for a single run!\r\n");
	}
}

/**
 * Returns true if the mouse is within the side panel.
 */
static inline bool is_mouse_in_side(int x, int y)
{
	return (x >= SIDE_X && x < SIDE_MX) && (y >= SIDE_Y && y < SIDE_MY);
}

/**
 * Returns the id of the audio file entry in which the mouse is currently
 * positioned.
 */
static inline int get_element_id(int x, int y)
{
	if (is_mouse_in_side(x, y))
		return (y - SIDE_Y) / SIDE_ELEM_MY;

	return -1;
}

/**
 * Returns the button id associated with the given coordinates.
 * The button id is the same for any button of the same type within each
 * element, so users of this function may check which element is selected
 * before calling this function.
 */
static inline button_id_t get_button_id(int x, int y)
{
int relx, rely;					// Position relative to the element the mouse is in
button_id_t id = BUTTON_INVALID;// The type of the selected button

	if(!is_mouse_in_side(x, y)) return BUTTON_INVALID;

	relx = x - SIDE_X;
	rely = (y - SIDE_Y) % SIDE_ELEM_MY;

	if (CHECK_BUTTON_POSX(relx, PLAY) && CHECK_BUTTON_POSY(rely, PLAY))
	{
		id = BUTTON_PLAY;
	}
	else if (CHECK_BUTTON_POSY(rely, ROW))
	{
		// Mouse Y is compliant with the '+' or '-' buttons

		if (CHECK_BUTTON_POSX(relx, VOL_DOWN))
		{
			id = BUTTON_VOL_DOWN;
		}
		else if (CHECK_BUTTON_POSX(relx, VOL_UP))
		{
			id = BUTTON_VOL_UP;
		}
		else if (CHECK_BUTTON_POSX(relx, PAN_DOWN))
		{
			id = BUTTON_PAN_DOWN;
		}
		else if (CHECK_BUTTON_POSX(relx, PAN_UP))
		{
			id = BUTTON_PAN_UP;
		}
		else if (CHECK_BUTTON_POSX(relx, FRQ_DOWN))
		{
			id = BUTTON_FRQ_DOWN;
		}
		else if (CHECK_BUTTON_POSX(relx, FRQ_UP))
		{
			id = BUTTON_FRQ_UP;
		}
	}

	return id;
}

/**
 * Handles a click operation on the button identified by button_id within the
 * element element_id. If parameters are invalid, this function does nothing.
 */
static inline void handle_click(int button_id, int element_id)
{
	switch(button_id)
	{
	case BUTTON_PLAY:
		audio_play_file(element_id);
		break;
	case BUTTON_VOL_UP:
		audio_volume_up(element_id);
		break;
	case BUTTON_VOL_DOWN:
		audio_volume_down(element_id);
		break;
	case BUTTON_PAN_UP:
		audio_panning_up(element_id);
		break;
	case BUTTON_PAN_DOWN:
		audio_panning_down(element_id);
		break;
	case BUTTON_FRQ_UP:
		audio_frequency_up(element_id);
		break;
	case BUTTON_FRQ_DOWN:
		audio_frequency_down(element_id);
		break;
	default:
		// Do nothing
		break;
	}
}

/**
 * Checks if the user has pressed the mouse on any button on the screen and if
 * so performs the requested action.
 */
static inline void handle_mouse_input()
{
int		pos, x, y, elem_id;
bool	pressed;

static button_id_t button_hover;
static button_id_t button_hover_past = -1;

static bool pressed_past = false;

static struct timespec next_click_time;
static struct timespec current_time;

	if(poll_mouse() || !mouse_on_screen()) return;	// No mouse driver installed
													// or mouse is not on screen

	pos		= mouse_pos;
	pressed	= mouse_b & 1;

	// TODO: Move these operations to a macro.
	x = pos >> 16;
	y = pos & 0x0000FFFF;

	button_hover	= get_button_id(x, y);
	elem_id			= get_element_id(x, y);

	if (button_hover == BUTTON_INVALID)
	{
		// Do nothing
	}
	else if (button_hover != button_hover_past)
	{
		// On a different position, a button event is valid only if it is a
		// click, if long press/drag then the previous value should be equal to
		// the current one and the event will be ignored.
		if (pressed && !pressed_past)
		{
#define MOUSE_DELAY_LONG	(500)
#define MOUSE_DELAY_SHORT	(15)
			handle_click(button_hover, elem_id);
			clock_gettime(CLOCK_MONOTONIC, &next_click_time);
			time_add_ms(&next_click_time, MOUSE_DELAY_LONG);
									// TODO: Move this value to a global constant.
		}
	} else
	{
		// On the same position we can do a different action wether the event
		// is a simple click or it is a long press. If it is a long press then
		// after an initial delay we will handle each mouse event as a new event
		// frequently. The first time the delay is bigger, following times the
		// delay is much smaller, basically like the original Tetris game.
		if (pressed && !pressed_past)
		{
			// First click is handled
			handle_click(button_hover, elem_id);
			clock_gettime(CLOCK_MONOTONIC, &next_click_time);
			time_add_ms(&next_click_time, MOUSE_DELAY_LONG);
									// TODO: Consider writing an inline function to do this job.
		} else if (pressed && pressed_past)
		{
			// On the same potion, if long press then timers come into game
			clock_gettime(CLOCK_MONOTONIC, &current_time);

			if (time_cmp(current_time, next_click_time) >= 0)
			{
				// Another click has to be handled, even if it is in fact a long
				// press
				handle_click(button_hover, elem_id);
				time_copy(&next_click_time, current_time);
				time_add_ms(&next_click_time, MOUSE_DELAY_SHORT);
			}
		}
	}

	pressed_past = pressed;
	button_hover_past = button_hover;
}

//@}

// -----------------------------------------------------------------------------
//                           PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------


int video_init()
{
int err;
	err = ptask_mutex_init(&gui_state.mutex);
	if (err) return err;

	set_color_depth(COLOR_MODE);

	return err;
}

// NOTE: Some of these functions may be moved to private part.

int gui_graphic_mode_init()
{
int err;

	err = set_gfx_mode(GFX_AUTODETECT_WINDOWED, WIN_MX, WIN_MY, 0, 0);
	if(err) return err;

	set_close_button_callback(close_button_proc);

	err = static_interface_init();

	return err;
}

void gui_graphic_mode_exit()
{
#ifndef NDEBUG
int err;
	// In debug mode, I assert that everything goes well.
	err = set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
	assert(err == 0);
#else
	set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
#endif
}


// -----------------------------------------------------------------------------
//                                  TASKS
// -----------------------------------------------------------------------------


void* gui_task(void* arg) {
ptask_t*	tp;			// task pointer
// int			task_id;	// task identifier

// Local variables
int err;

	tp = STATIC_CAST(ptask_t*, arg);
	// task_id = ptask_get_id(tp);

	// Variables initialization and initial computation

	err = gui_graphic_mode_init();
	if (err)
		abort_on_error("Could not initialize graphic mode.");

	// FIXME: Clear virtual screen content!

	ptask_start_period(tp);

	while (!main_get_tasks_terminate())
	{
		screen_refresh();

		if (ptask_deadline_miss(tp))
			printf("TASK_GUI missed %d deadlines!\r\n", ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup
	gui_graphic_mode_exit();

	return NULL;
}



void* user_interaction_task(void* arg) {
ptask_t*	tp;			// task pointer
// int			task_id;	// task identifier

// Local variables
int err;

	tp = STATIC_CAST(ptask_t*, arg);
	// task_id = ptask_get_id(tp);

	// Variables initialization and initial computation

	err = install_keyboard();
	if (err)
		abort_on_error("Could not initialize the keyboard.");

	err = install_mouse();
	if (err < 0)
		abort_on_error("Could not initialize the mouse.");

	enable_hardware_cursor();

	ptask_mutex_lock(&gui_state.mutex);
	gui_state.mouse_initialized = true;
	ptask_mutex_unlock(&gui_state.mutex);

	ptask_start_period(tp);

	while (!main_get_tasks_terminate())
	{
		handle_keyboard_inputs();

		handle_mouse_input();

		if (ptask_deadline_miss(tp))
			printf("TASK_UI missed %d deadlines!\r\n", ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup
	remove_mouse();
	remove_keyboard();

	audio_stop();

	ptask_mutex_lock(&gui_state.mutex);

	gui_state.mouse_initialized = false;
	gui_state.mouse_shown = false;

	ptask_mutex_unlock(&gui_state.mutex);

	return NULL;
}
