#include <stdbool.h>
#include <stdio.h>
#include <allegro.h>

#include <assert.h>

#include "api/std_emu.h"
#include "api/time.h"
#include "api/ptask.h"

#include "constants.h"

#include "main.h"
#include "audio.h"
#include "video.h"



/* -----------------------------------------------------------------------------
 * CONSTANTS
 * -----------------------------------------------------------------------------
 */

#define BITMAP_RES_FOLDER		"res/"
#define BITMAP_BACKGROUND_PATH	BITMAP_RES_FOLDER "background.bmp"
#define BITMAP_S_ELEMENT_PATH	BITMAP_RES_FOLDER "element.bmp"
#define BITMAP_M_ELEMENT_PATH	BITMAP_RES_FOLDER "element_midi.bmp"

#define STR_VOLUME				"Volume"
#define STR_PANNING				"Panning"
#define STR_FREQUENCY			"Base Frequency"


/* -----------------------------------------------------------------------------
 * DATA TYPES
 * -----------------------------------------------------------------------------
 */

// Contains all static elements of the gui
typedef struct __GUI_STATIC_STRUCT
{
	BITMAP* background;		// Static background of the interface

	BITMAP* element_sample;	// Static background of an element of the
							// interface representing an opened sample

	BITMAP* element_midi;	// Static background of an element of the
							// interface representing an opened midi

} gui_static_t;

// Global state of the module
typedef struct __GUI_STRUCT
{
	// NOTE: When modifying the main_state_t data structure, consider
	// which state should be adopted as default state.

	BITMAP*		virtual_screen;	// Virtual screen, used to handle all gui
								// operations, it is copied in the actual screen
								// when refreshed

	gui_static_t static_screen;	// Contains all the gui bitmaps

	bool		initialized;	// Tells if this interface has been initialized
								// i.e. all bitmaps are loaded and so on

	bool		mouse_initialized;
								// Tells if the mouse handler has been
								// initialized
	bool		mouse_shown;	// Tells if the show_mouse function has been
								// called on the current screen

	ptask_mutex_t mutex;		// This mutex is used to protect the access of
								// the mouse flags
} gui_state_t;

typedef enum __BUTTON_ID_ENUM
{
	BUTTON_INVALID = -1,
	BUTTON_PLAY,
	BUTTON_VOL_DOWN,
	BUTTON_VOL_UP,
	BUTTON_PAN_DOWN,
	BUTTON_PAN_UP,
	BUTTON_FRQ_DOWN,
	BUTTON_FRQ_UP,
} button_id_t;

/* -----------------------------------------------------------------------------
 * GLOBAL VARIABLES
 * -----------------------------------------------------------------------------
 */

gui_state_t gui_state =
{
	.initialized = false,
	.mouse_initialized = false,
	.mouse_shown = false,
};

/* -----------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * If not previously initialized, loads all the interface static members.
 */
int _static_interface_init()
{
BITMAP* 		bitmap_ptr;		// Temporarily points to a BITMAP
gui_static_t	static_screen;	// Holds all the static interface elements

	if(gui_state.initialized)
		return 0;

	bitmap_ptr = load_bitmap(BITMAP_BACKGROUND_PATH, NULL);
	if (bitmap_ptr == NULL) return EINVAL;

	static_screen.background = bitmap_ptr;

	bitmap_ptr = load_bitmap(BITMAP_S_ELEMENT_PATH, NULL);
	if (bitmap_ptr == NULL) return EINVAL;

	// Adds texts to the base element bitmap, so that the only font used in the
	// graphic interface is the Allegro one
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

	static_screen.element_midi = bitmap_ptr;

	gui_state.static_screen = static_screen;
	gui_state.virtual_screen = create_bitmap(WIN_MX, WIN_MY);
	gui_state.initialized = true;

	return 0;
}

/*
 * Copies the background onto the virtual screen.
 */
void _draw_background()
{
	blit(gui_state.static_screen.background, gui_state.virtual_screen,
		0, 0, 0, 0, WIN_MX, WIN_MY);
}

/*
 * Draws the given index element, assuming that it is an audio sample element.
 */
void _draw_side_element_sample(int index)
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

/*
 * Draws the given index element, assuming that it is a midi element.
 */
void _draw_side_element_midi(int index)
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


/*
 * Draws the given index element.
 */
void _draw_side_element(int index)
{
	switch (audio_get_type(index))
	{
	case AUDIO_TYPE_SAMPLE:

		_draw_side_element_sample(index);

		break;
	case AUDIO_TYPE_MIDI:

		_draw_side_element_midi(index);

		break;
	default:
		assert(false);
	}
}

/*
 * Overwrites the sidebar on the virtual screen.
 */
void _draw_sidebar()
{
int num, i;

	num = audio_get_num_files();

	for (i = 0; i < num; ++i)
	{
		_draw_side_element(i);
	}
}

/*
 * Draws the FFT of the (last) recorded audio on the screen.
 */
void _draw_fft()
{
	// TODO: To implement this function, try to find again the code that you
	// wrote more than one year ago!
}

/*
#define TIME_P	(PADDING*2)				// time graph padding
#define TIME_X	(0)						// time graph position x
#define TIME_Y	(FFT_MY+TIME_P)			// time graph position y
#define TIME_MX	(SIDE_X)				// time graph max x
#define TIME_MY (FOOTER_Y)				// time graph max y
*/

#define TIME_WIDTH	(TIME_MX - TIME_X - 2*TIME_P)
#define TIME_HEIGHT	(TIME_MY - TIME_Y - 2*TIME_P)

int _compute_last_amplitude()
{
int buffer_index;
int i;
short* buffer;
double amplitude = 0;

	int dim = audio_get_last_record(&buffer, &buffer_index);

	if (dim > 0)
	{
		for (i = 0; i < dim; ++i)
		{
			amplitude +=
				STATIC_CAST(double, buffer[i]) * STATIC_CAST(double, buffer[i]);
		}

		audio_free_last_record(buffer_index);

		amplitude /= (double) dim;
	}

	return (int) amplitude;
}

/*
 * Draws the amplitude of the input signal on the screen.
 */
void _draw_amplitude()
{
// TODO: Consider changing the organization of this function and to adopt more
// global variables/constants
static BITMAP *amplitude_bitmap;
static bool first = true;
#define AMPLITUDE_SPEED 1
#define AMPLITUDE_MAX_HEIGHT (TIME_MY - TIME_P - 1) - (TIME_Y + 3 * TIME_P)
#define AMPLITUDE_MAX			1000000
int amplitude;

	if (first)
	{
		amplitude_bitmap = create_bitmap(TIME_WIDTH, TIME_HEIGHT);

		blit(gui_state.virtual_screen, amplitude_bitmap,
			TIME_X + TIME_P,
			TIME_Y + TIME_P,
			0,
			0,
			TIME_WIDTH,
			TIME_HEIGHT);

		first = false;
	}

	amplitude = _compute_last_amplitude();

	blit(amplitude_bitmap, gui_state.virtual_screen,
			AMPLITUDE_SPEED,
			0,
			TIME_X + TIME_P,
			TIME_Y + TIME_P,
			TIME_WIDTH - AMPLITUDE_SPEED,
			TIME_HEIGHT);

#define AMP_TO_HEIGHT(ampl) \
	STATIC_CAST(int, \
		((STATIC_CAST(double,AMPLITUDE_MAX_HEIGHT) * \
		STATIC_CAST(double,amplitude)) / \
		STATIC_CAST(double,AMPLITUDE_MAX)\
		) \
	)

	amplitude = AMP_TO_HEIGHT(amplitude);

	amplitude = (amplitude > AMPLITUDE_MAX_HEIGHT) ?
		AMPLITUDE_MAX_HEIGHT : amplitude;

	// NOTE: This code should be adopted with a better plotting function.
	// The function shall print on a width equal to AMPLITUDE_SPEED pixels
	// on each execution a value that is proportional to the amplitude collected
	// above. The function shall use scaling and other constants to control its
	// behavior.
	rectfill(gui_state.virtual_screen,
			 TIME_MX - TIME_P - AMPLITUDE_SPEED - 1,
			 TIME_MY - TIME_P - 1 - amplitude,
			 TIME_MX - TIME_P - 1,
			 TIME_MY - TIME_P - 1,
			 COLOR_ACCENT);

	blit(gui_state.virtual_screen, amplitude_bitmap,
		TIME_X + TIME_P,
		TIME_Y + TIME_P,
		0,
		0,
		TIME_WIDTH,
		TIME_HEIGHT);

}

/*
 * Calls Allegro show_mouse(screen) if the mouse module has been initialized,
 * but only once.
 */
void _show_mouse()
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


void _screen_refresh()
{
	_draw_background();

	_draw_sidebar();

	_draw_fft();

	_draw_amplitude();

	blit(gui_state.virtual_screen, screen, 0, 0, 0, 0, WIN_MX, WIN_MY);

	_show_mouse();
}

/*
 * Asynchronous procedure called by Allegro when the close button of the
 * window is pressed.
 * Called only when the graphic mode is active.
 */
void _close_button_proc()
{
	main_terminate_tasks();
}

#define MAX_KEY_COMMANDS	256

void _handle_num_key(int num)
{
	num = num == 0 ? 9 : num-1;

	if (num < audio_get_num_files())
	{
		audio_play_file(num);
	}
}

/*
 * Checks if the user has given commands to the program while in graphic mode.
 * Commands are specified by single key press.
 */
void _handle_keyboard_inputs()
{
int count;
int key;
int scancode;

	for(count = 0; keypressed() && count < MAX_KEY_COMMANDS; count++)
	{
		key = readkey();

		scancode = key >> 8;

		if (scancode >= KEY_0 && scancode <= KEY_9)
			_handle_num_key(scancode - KEY_0);
		else
		{
			switch(key >> 8)
			{
			case KEY_Q:
				main_terminate_tasks();
				break;
			// IDEA: Implement global volume controls, Up/Down.
			default:
				// Do nothing
				break;
			}
		}
	}

	if (keypressed())
	{
		printf("UI_TASK: Too much keyboard commands!\r\n");
	}
}

bool _is_mouse_in_side(int x, int y)
{
	return (x >= SIDE_X && x < SIDE_MX) && (y >= SIDE_Y && y < SIDE_MY);

}

int _get_element_id(int x, int y)
{
	if (_is_mouse_in_side(x, y))
		return (y - SIDE_Y) / SIDE_ELEM_MY;

	return -1;
}

button_id_t _get_button_id(int x, int y)
{
int relx, rely;	// Position relative to the element the mouse is in
button_id_t id = BUTTON_INVALID;

	if(!_is_mouse_in_side(x, y)) return BUTTON_INVALID;

	relx = x - SIDE_X;

	rely	= (y - SIDE_Y) % SIDE_ELEM_MY;

	if (CHECK_BUTTON_POSX(relx, PLAY) && CHECK_BUTTON_POSY(rely, PLAY))
	{
		return BUTTON_PLAY;
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

void _handle_click(int button_id, int element_id)
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

/*
 * Checks if the user has pressed the mouse on any button on the screen and if
 *so performs the requested action.
 */
void _handle_mouse_input()
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

	pos = mouse_pos;
	pressed = mouse_b & 1;

	// TODO: Move these operations to a macro.
	x = pos >> 16;
	y = pos & 0x0000FFFF;

	button_hover = _get_button_id(x, y);
	elem_id = _get_element_id(x, y);

	if (button_hover == BUTTON_INVALID)
	{
		// Do nothing
	}
	else if (button_hover != button_hover_past)
	{
		// On a different position, a button event is valid only if first click
		if (pressed && !pressed_past)
		{
			_handle_click(button_hover, elem_id);
			clock_gettime(CLOCK_MONOTONIC, &next_click_time);
			time_add_ms(&next_click_time, 500);
									// TODO: Move this value to a global constant.
		}
	} else
	{
		if (pressed && !pressed_past)
		{
			// First click is handled
			_handle_click(button_hover, elem_id);
			clock_gettime(CLOCK_MONOTONIC, &next_click_time);
			time_add_ms(&next_click_time, 500);
									// TODO: Same as above, also consider
									// writing an inline function to do this job.
		} else if (pressed && pressed_past)
		{
			// On the same potion, if long press then timers come into game
			clock_gettime(CLOCK_MONOTONIC, &current_time);

			if (time_cmp(current_time, next_click_time) >= 0)
			{
				// Another click has to be handled, even if it is in fact a long
				// press
				_handle_click(button_hover, elem_id);

				time_copy(&next_click_time, current_time);
				time_add_ms(&next_click_time, 15);
									// TODO: Move this value to a global constant.
			}
		}
	}

	pressed_past = pressed;
	button_hover_past = button_hover;

}



/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Initializes the mutex and the other required data structures.
 */
int video_init()
{
int err;
	err = ptask_mutex_init(&gui_state.mutex);
	if (err) return err;

	set_color_depth(COLOR_MODE);

	return err;
}

// NOTE: Some of these functions may be moved to private part.

/*
 * Initializes the graphic mode by creating a new window.
 */
int gui_graphic_mode_init()
{
int err;

	err = set_gfx_mode(GFX_AUTODETECT_WINDOWED, WIN_MX, WIN_MY, 0, 0);
	if(err) return err;

	set_close_button_callback(_close_button_proc);

	err = _static_interface_init();

	return err;
}

/*
 * Destroys the window.
 */
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


/* -----------------------------------------------------------------------------
 * TASKS
 * -----------------------------------------------------------------------------
 */

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

	ptask_start_period(tp);

	while (!main_get_tasks_terminate())
	{
		_screen_refresh();

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
		_handle_keyboard_inputs();

		_handle_mouse_input();

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
