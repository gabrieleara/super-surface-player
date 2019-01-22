/**
 * @file constants.h
 * @brief All globally shared constants in the system
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 */


#ifndef CONSTANTS_H
#define CONSTANTS_H

// -----------------------------------------------------------------------------
//                         GLOBAL SHARED CONSTANTS
// -----------------------------------------------------------------------------

#define MAX_CHAR_BUFFER_SIZE	256		///< Size of any char buffer used in the system
#define MAX_DIRECTORY_LENGTH	256		///< Maximum length of a directory name

// -----------------------------------------------------------------------------
//                           RECORDING CONSTANTS
// -----------------------------------------------------------------------------

#define AUDIO_DESIRED_RATE		44100	///< Desired acquisition rate (Hz)

#define AUDIO_DESIRED_FRAMES	4096
										///< Desired acquisition buffer dimension.
										///< NOTICE: that the bigger this is,
										///< the bigger the latency of the system

#define AUDIO_FRAMES_TO_HALFCOMPLEX(frames) ((frames + 1) / 2 - 1)
										///< Converts the number of acquired
										///< frames to the number of values of
										///< the corresponding magnitude-only
										///< half-complex FFT.
										///< See FFTW documentation for further
										///< details.

#define AUDIO_DESIRED_HALFCOMPLEX	AUDIO_FRAMES_TO_HALFCOMPLEX(AUDIO_DESIRED_FRAMES)
										///< Desired dimension of the
										///< magnitude-only half-complex FFT
										///< buffer.

#define AUDIO_NUM_BUFFERS		10
										///< Number of buffers related to audio
										///< acquisition

/// Converts a certain number of frames in milliseconds, given current
/// capture rate.
#define FRAMES_TO_MS(frames,rate) \
	((1000L * STATIC_CAST(long,frames)) / STATIC_CAST(long,rate))

/// The desired period of the audio acquisition task
#define AUDIO_DESIRED_PERIOD \
	FRAMES_TO_MS(AUDIO_DESIRED_FRAMES,AUDIO_DESIRED_RATE)

// TODO: Add dimensions of buffers used for data analysis

// -----------------------------------------------------------------------------
//                           GRAPHIC CONSTANTS
// -----------------------------------------------------------------------------

//-------------------------------------------------------------
// Colors
//-------------------------------------------------------------

#define COLOR_MODE		(32)		///< default color mode

#define COLOR_WHITE		(0xFFFFFF)	///< white
#define COLOR_BKG		(0xF0F0F0)	///< light gray background
#define COLOR_PRIM_DARK	(0x388E3C)	///< dark primary color
#define COLOR_PRIM		(0x4CAF50)	///< primary color
#define COLOR_PRIM_LIGH	(0xC8E6C9)	///< light prmary color
#define COLOR_ACCENT	(0x8BC34A)	///< accent color
#define COLOR_TEXT		(0xFFFFFF)	///< text/icons over a primary or accent color
#define COLOR_TEXT_PRIM	(0x212121)	///< primary text color, over a background
#define COLOR_TEXT_SECN	(0x757575)	///< secondary text color, over a prim or accent
#define COLOR_DIVIDER	(0xBDBDBD)	///< color used for dividers

//-------------------------------------------------------------
// Window dimensions
//-------------------------------------------------------------

#define WIN_X		(0)				///< start of coordinates x
#define WIN_Y		(0)				///< start of coordinates y
#define WIN_MX		(1280)			///< window x resolution
#define WIN_MY		(672)			///< window y resolution

#define PADDING		(12)
									///< base value for padding that can be
									///< used by child elements

//-------------------------------------------------------------
// The Footer will contain some messages printed by the program
//-------------------------------------------------------------

#define FOOTER_X	(WIN_X)			///< footer position x
#define FOOTER_Y	(624)			///< footer position y
#define FOOTER_MX	(WIN_MX)		///< footer max x
#define FOOTER_MY	(WIN_MY)		///< footer max y

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

#define SIDE_X		(WIN_MX * 7/10)		///< side panel position x
#define SIDE_Y		(0)					///< side panel position y
#define SIDE_MX		(WIN_MX)			///< side panel max x
#define SIDE_MY		(FOOTER_Y)			///< side panel max y


#define SIDE_ELEM_X	(SIDE_X)				///< side panel element position x
#define SIDE_ELEM_MX (SIDE_MX-SIDE_X)		///< side panel max x
#define SIDE_ELEM_MY ((SIDE_MY-SIDE_Y)/8)	///< side panel max y

// TODO: document next constants

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



// BUTTONS, NOTICE: these values are relative to the elem position!

#define BUTTON_PLAY_X		(360)
#define BUTTON_PLAY_Y		(10)
#define BUTTON_PLAY_MX		(374)
#define BUTTON_PLAY_MY		(30)

#define BUTTON_ROW_Y		(50)
#define BUTTON_ROW_MY		(66)

#define BUTTON_VOL_DOWN_X	(40)
#define BUTTON_VOL_DOWN_MX	(56)
#define BUTTON_PAN_DOWN_X	(40 + 128)
#define BUTTON_PAN_DOWN_MX	(56 + 128)
#define BUTTON_FRQ_DOWN_X	(40 + 256)
#define BUTTON_FRQ_DOWN_MX	(56 + 256)

#define BUTTON_VOL_UP_X		(102)
#define BUTTON_VOL_UP_MX	(118)
#define BUTTON_PAN_UP_X		(102 + 128)
#define BUTTON_PAN_UP_MX	(118 + 128)
#define BUTTON_FRQ_UP_X		(102 + 256)
#define BUTTON_FRQ_UP_MX	(118 + 256)

#define PASTER(button, dim) BUTTON_ ## button ## _ ## dim
#define CHECK_BUTTON_POSY(posy, button) (posy >= PASTER(button,Y) && posy < PASTER(button,MY))
#define CHECK_BUTTON_POSX(posx, button) (posx >= PASTER(button,X) && posx < PASTER(button,MX))




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

// FIXME: change the horrible "graph" word
#define FFT_P		(PADDING * 2)			///< fft graph padding
#define FFT_X		(0 + FFT_P)				///< fft graph position x
#define FFT_Y		(0 + FFT_P)				///< fft graph position y
#define FFT_MX		(SIDE_X - FFT_P)		///< fft graph max x
#define FFT_MY		(FOOTER_Y/2 - FFT_P)	///< fft graph max y

#define FFT_PLOT_X		(FFT_X + FFT_P)
#define FFT_PLOT_Y		(FFT_Y + FFT_P)
#define FFT_PLOT_MX		(FFT_MX - FFT_P)
#define FFT_PLOT_MY		(FFT_MY - FFT_P)

#define FFT_PLOT_WIDTH	(FFT_PLOT_MX - FFT_PLOT_X - 1)
#define FFT_PLOT_HEIGHT	(FFT_PLOT_MY - FFT_PLOT_Y)

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
	The only thing drawn by the program dynamically is the time plot, so the
	dimensions specified here are the borders of the plot.
*/

// TODO: change comments
#define TIME_P	(PADDING*2)				///< time plot padding
#define TIME_X	(0)						///< time plot position x
#define TIME_Y	(FOOTER_Y/2 + TIME_P)	///< time plot position y
#define TIME_MX	(SIDE_X)				///< time plot max x
#define TIME_MY	(FOOTER_Y)				///< time plot max y

#define TIME_PLOT_X		(TIME_X + TIME_P)
#define TIME_PLOT_Y		(TIME_Y + TIME_P)
#define TIME_PLOT_MX	(TIME_MX - TIME_P)
#define TIME_PLOT_MY	(TIME_MY - TIME_P)

#define TIME_PLOT_WIDTH		(TIME_PLOT_MX - TIME_PLOT_X - 1)///< time plot width
#define TIME_PLOT_HEIGHT	(TIME_PLOT_MY - TIME_PLOT_Y)	///< time plot height

#define TIME_SPEED			(1)		///< The speed at which the time plot moves
#define TIME_MAX_HEIGHT		((TIME_PLOT_HEIGHT - 1 - (2 * TIME_P)))
									///< The maximum height of the time plot content
#define TIME_MAX_AMPLITUDE	(1000000)
									///< The amplitude which corresponds to
									///< the maximum height



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
	#define GET_PRIO(prio) (prio)	///< If not in debug mode, this does nothing
#else
	#define GET_PRIO(prio) (0)		///< In debug mode real time scheduling is
#endif								///< disabled, so priority must be zero

// TODO: Periods of tasks should be chosen in a more rigorous manneer, perform
// a scheduling analysis once all tasks are correctly defined. Also, priority
// between tasks should be defined in terms of the results of said
// schedulability analysis.

// GUI TASK

#define TASK_GUI_WCET		(WCET_UNKNOWN)
#define TASK_GUI_PERIOD		(30)	///< a little more than 30 fps
#define TASK_GUI_DEADLINE	(30)	///< same as period
#define TASK_GUI_PRIORITY	(1)		///< lowest priority, missing a frame is not a big deal

// USER INTERACTION TASK

#define TASK_UI_WCET		(WCET_UNKNOWN)
#define TASK_UI_PERIOD		(10)
#define TASK_UI_DEADLINE	(10)
#define TASK_UI_PRIORITY	(2)

// MICROPHONE TASK

#define TASK_MIC_WCET		(WCET_UNKNOWN)
#define TASK_MIC_PERIOD		(AUDIO_DESIRED_PERIOD/10)
#define TASK_MIC_DEADLINE 	(TASK_MIC_PERIOD)
#define TASK_MIC_PRIORITY	(4)

// FFT TASK

#define TASK_FFT_WCET		(WCET_UNKNOWN)
#define TASK_FFT_PERIOD		(AUDIO_DESIRED_PERIOD / 10)
#define TASK_FFT_DEADLINE	(TASK_FFT_PERIOD)
#define TASK_FFT_PRIORITY	(3)

// TODO: Define characteristics of each ANALYSIS TASK


#endif
