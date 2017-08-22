#ifndef CONSTANTS_H
#define CONSTANTS_H

//-------------------------------------------------------------
// GLOBAL SHARED CONSTANTS
//-------------------------------------------------------------

#define MAX_BUFFER_SIZE			256		// Size of any buffer used in the system
#define MAX_DIRECTORY_LENGTH	256		// Maximum length of a directory name


//-------------------------------------------------------------
// GRAPHIC CONSTANTS
//-------------------------------------------------------------

//-------------------------------------------------------------
// COLORS
//-------------------------------------------------------------

#define COLOR_MODE	(32)			// default color mode

// TODO: change
#define COLOR_WHITE		(0xFFFFFF)	// white
#define COLOR_BKG		(0xF0F0F0)	// light gray background
#define COLOR_PRIM_DARK	(0x388E3C)	// dark primary color
#define COLOR_PRIM		(0x4CAF50)	// primary color
#define COLOR_PRIM_LIGH	(0xC8E6C9)	// light prmary color
#define COLOR_ACCENT	(0x8BC34A)	// accent color
#define COLOR_TEXT		(0xFFFFFF)	// text/icons over a primary or accent color
#define COLOR_TEXT_PRIM	(0x212121)	// primary text color, over a background
#define COLOR_TEXT_SECN	(0x757575)	// secondary text color, over a prim or accent
#define COLOR_DIVIDER	(0xBDBDBD)	// color used for dividers

//-------------------------------------------------------------
// Window dimensions
//-------------------------------------------------------------

#define WIN_X		(0)					// start of coordinates x
#define WIN_Y		(0)					// start of coordinates y
#define WIN_MX		(1280)				// window x resolution
#define WIN_MY		(672)				// window y resolution

#define PADDING		(12)				// base value for padding that can be
										// used by child elements

//-------------------------------------------------------------
// The Footer will contain some messages printed by the program
//-------------------------------------------------------------

#define FOOTER_X	(WIN_X)				// footer position x
#define FOOTER_Y	(624)				// footer position y
#define FOOTER_MX	(WIN_MX)			// footer max x
#define FOOTER_MY	(WIN_MY)			// footer max y

/*
	 __________________________________________________________
	|                                           |              |
	|                                           |              |
	|                                           |              |
	|                                           |     SIDE     |
	|                                           |     PANEL    |
	|                                           |              |
	|                                           |              |
	|                                           |              |
	|                                           |              |
	|___________________________________________|______________|
	|__________________________________________________________|

	The side panel contains the list of all the files opened by the program,
	colors will provide informations when a file is executing.
*/

#define SIDE_X		(WIN_MX * 7/10)		// side panel position x
#define SIDE_Y		(0)					// side panel position y
#define SIDE_MX		(WIN_MX)			// side panel max x
#define SIDE_MY		(FOOTER_Y)			// side panel max y


#define SIDE_ELEM_X	(SIDE_X)			// side panel element position x
#define SIDE_ELEM_MX (SIDE_MX-SIDE_X)	// side panel max x
#define SIDE_ELEM_MY ((SIDE_MY-SIDE_Y)/8)// side panel max y

#define SIDE_ELEM_NAME_X	(SIDE_ELEM_X + 12)
#define SIDE_ELEM_NAME_Y	(20)

#define SIDE_ELEM_VOL_LABEL_X	(12)
#define SIDE_ELEM_VOL_LABEL_Y	(40)
#define SIDE_ELEM_PAN_LABEL_X	(12+128)
#define SIDE_ELEM_PAN_LABEL_Y	(40)
#define SIDE_ELEM_FRQ_LABEL_X	(12+256-8)
#define SIDE_ELEM_FRQ_LABEL_Y	(40)

#define SIDE_ELEM_VAL_Y		(54)

#define SIDE_ELEM_VOL_X		(SIDE_ELEM_X + 64)
#define SIDE_ELEM_PAN_X		(SIDE_ELEM_X + 64+128)
#define SIDE_ELEM_FRQ_X		(SIDE_ELEM_X + 64+256)



/*
	 __________________________________________________________
	|                                           |              |
	|                 FFT PANEL                 |              |
	|                                           |              |
	|                                           |     SIDE     |
	|___________________________________________|     PANEL    |
	|                                           |              |
	|                                           |              |
	|                                           |              |
	|                                           |              |
	|___________________________________________|______________|
	|__________________________________________________________|

	The fft panel contains the fft of the sound received from the recorder.
	The scale and background are contained in a bitmap file, thus they are
	static.
	The only thing drawn by the program dynamically is the fft graph, so the
	dimensions specified here are the borders of the graph.
*/

#define FFT_P		(PADDING * 2)		// fft graph padding
#define FFT_X		(0 + FFT_P)			// fft graph position x
#define FFT_Y		(0 + FFT_P)			// fft graph position y
#define FFT_MX		(SIDE_X - FFT_P)	// fft graph max x
#define FFT_MY		(FOOTER_Y/2 - FFT_P)// fft graph max y

/*
	 __________________________________________________________
	|                                           |              |
	|                 FFT PANEL                 |              |
	|                                           |              |
	|                                           |     SIDE     |
	|___________________________________________|     PANEL    |
	|                                           |              |
	|                                           |              |
	|                TIME PANEL                 |              |
	|                                           |              |
	|___________________________________________|______________|
	|__________________________________________________________|

	The time panel contains the waveform of the sound received from the
	recorder in the time domain.
	The scale and background are contained in a bitmap file, thus they are
	static.
	The only thing drawn by the program dynamically is the time graph, so the
	dimensions specified here are the borders of the graph.
*/

#define TIME_P	(PADDING*2)				// time graph padding
#define TIME_X	(0)						// time graph position x
#define TIME_Y	(FFT_MY)				// time graph position y
#define TIME_MX	(HOR_PANEL_MX)			// time graph max x
#define TIME_MY	(HOR_PANEL_H + TIME_Y)	// time graph max y

//-------------------------------------------------------------
// TASKS CONSTANTS
//-------------------------------------------------------------

// The tasks are: gui, user interaction, microphone, fft and analysis.
#define TASK_GUI	(0)
#define TASK_UI		(1)
#define TASK_MIC	(2)
#define TASK_FFT	(3)
#define TASK_ALS	(4)

#define	TASK_NUM	(5)

#define WCET_UNKNOWN (0)

#ifdef NDEBUG
	#define GET_PRIO(prio) (prio)	// If not in debug mode, this does nothing
#else
	#define GET_PRIO(prio) (0)		// In debug mode real time scheduling is
#endif								// disabled, so priority must be zero

// GUI TASK
#define TASK_GUI_WCET		(WCET_UNKNOWN)
#define TASK_GUI_PERIOD		(30)	// a little more than 30 fps
#define TASK_GUI_DEADLINE	(30)	// same as period
#define TASK_GUI_PRIORITY	(1)		// lowest priority, missing a frame is not a
									// big deal

// USER INTERACTION TASK

#define TASK_UI_WCET		(WCET_UNKNOWN)
#define TASK_UI_PERIOD		(30)	// same as gui TODO: change
#define TASK_UI_DEADLINE	(30)	// TODO: same as period?
#define TASK_UI_PRIORITY	(2)		// TODO: change

// MICROPHONE TASK

#define TASK_MIC_WCET		(WCET_UNKNOWN)
#define TASK_MIC_PERIOD		(8)		// Must be at least 100 times per second
#define TASK_MIC_DEADLINE	(8)		// TODO: same as period?
#define TASK_MIC_PRIORITY	(2)		// TODO: change

// MICROPHONE TASK

#define TASK_MIC_WCET		(WCET_UNKNOWN)
#define TASK_MIC_PERIOD		(8)		// TODO: decide
#define TASK_MIC_DEADLINE	(8)		// TODO: same as period?
#define TASK_MIC_PRIORITY	(2)		// TODO: change

#endif