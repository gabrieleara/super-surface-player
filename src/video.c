#include <stdio.h>
#include <allegro.h>

#include <assert.h>

#include "api/std_emu.h"
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

	BITMAP*		virtual_screen;	// Virtual screen, used to handle all gui
								// operations, it is copied in the actual screen
								// when refreshed

	gui_static_t static_screen;	// Contains all the gui bitmaps

	bool		initialized;	// Tells if this interface has been initialized
								// i.e. all bitmaps are loaded and so on

	// TODO
} gui_state_t;


/* -----------------------------------------------------------------------------
 * GLOBAL VARIABLES
 * -----------------------------------------------------------------------------
 */

gui_state_t gui_state =
{
	.initialized = false,
	// TODO:
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
	// TODO:
}

/*
 * Draws the amplitude of the input signal on the screen.
 */
void _draw_amplitude()
{
	// TODO:
}

void _screen_refresh()
{
	_draw_background();

	_draw_sidebar();

	_draw_fft();

	_draw_amplitude();

	blit(gui_state.virtual_screen, screen, 0, 0, 0, 0, WIN_MX, WIN_MY);
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



/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Initializes the graphic mode, by creating a new window and starting recording.
 */
int gui_graphic_mode_init()
{
int err;

	err = install_keyboard(); if (err) return err;

	set_color_depth(COLOR_MODE);

	err = set_gfx_mode(GFX_AUTODETECT_WINDOWED, WIN_MX, WIN_MY, 0, 0);
	if(err) return err;

	err = install_mouse(); if(err < 0) return EPERM;

	enable_hardware_cursor();
	show_mouse(screen);

	set_close_button_callback(_close_button_proc);

	err = _static_interface_init(); if (err) return err;

	return 0;
}

/*
 * Destroys the window and removes all mouse and keyboard handlers.
 */
void gui_graphic_mode_exit()
{
#ifndef NDEBUG
int err;
#endif

	remove_mouse();
	remove_keyboard();

#ifndef NDEBUG
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
int			task_id;	// task identifier

// Local variables
int err;

	tp = STATIC_CAST(ptask_t*, arg);
	task_id = ptask_get_id(tp);

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

