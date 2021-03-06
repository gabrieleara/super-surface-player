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
//                       CONFIGURABLE SHARED CONSTANTS
// -----------------------------------------------------------------------------

/**
 * @name Configurable shared properties
 */
//@{

// Constants in this section can be changed to modify the behavior of the
// program in a non-destructive way

#define AUDIO_DESIRED_RATE		(44100)	///< Desired acquisition rate (Hz)

#define AUDIO_ENABLE_PADDING	(1)
										///< 0 means no zero-padding, 1 means
										///< padding will be used and its size
										///< will depend on AUDIO_PADDING_RATIO
										///< constant value

#define AUDIO_PADDING_RATIO		(4)
										///< The ratio between the number of
										///< frames captured and the number of
										///< frames after padding is added
										///< (if enabled)

/// The treshold that is used to check whether the recorded audio corresponds to
/// a given audio sample
#define AUDIO_THRESHOLD			(0.3)

/// Desired number of frames contained in an audio sample.
/// It should be a power of two to make the FFT computation way faster.
/// NOTICE: the bigger this number, the bigger the latency.
#define AUDIO_DESIRED_FRAMES	(4*1024)

/// Increase this value to try to decrease the latency of the system, at the
/// cost of additional task wakeups.
/// ALSA publishes data with a period that can be manually configured in terms
/// of number of frames. If set to one, the configured period will be set equal
/// to AUDIO_DESIRED_FRAMES, which is the window used to analyze data.
/// If set to a bigger value (use only powers of two, since AUDIO_DESIRED_FRAMES
/// is a power of two), the period used for ALSA audio capture will be a
/// fraction of the default period. This requires the tasks that interact with
/// microphone data to be executed more frequently to capture published data,
/// but it will also reduce greatly the delay between the acquisition of the
/// audio and the reception of that same audio by the program, increasing its
/// responsiveness.
/// Tested correctly with the following values: 1, 2, 4, 8, 16
/// NOTICE: if AUDIO_APERIODIC is enabled, one of the tasks will become aperiodic,
/// but the tasks that analyze the FFTs can still be enhanced using this
/// constant.
#define AUDIO_LATENCY_REDUCER	(8)

/// The minimum delay between two samples to be recognized as the same sound,
/// in milliseconds. Increase this when you experience overlapping of the same
/// sound because of sustained input.
/// Minimum value is zero.
#define AUDIO_ANALYSIS_DELAY_MS	(500)

/// The amplitude which corresponds to the maximum height
#define TIME_MAX_AMPLITUDE		(1000000000/2)

/// The scaling of the FFT plot with respect to the computed energy value
#define FFT_PLOT_SCALING		(5.)

/// Uncomment this line to make the microphone task an aperiodic task, waking
/// it up using another faster task
// #define AUDIO_APERIODIC


//@}


// -----------------------------------------------------------------------------
//                         GLOBAL SHARED CONSTANTS
// -----------------------------------------------------------------------------

#define MAX_CHAR_BUFFER_SIZE	(256)	///< Size of any char buffer used in the system
#define MAX_DIRECTORY_LENGTH	(256)	///< Maximum length of a directory name

#define LOG_VERBOSE				(0x01)	///< Verbose logging enabled

// -----------------------------------------------------------------------------
//                           RECORDING CONSTANTS
// -----------------------------------------------------------------------------

/**
 * @name Recording-related constants
 */
//@{

#define AUDIO_MAX_FILES			(8)		///< The maximum number of opened audio files

/// Adds padding to the specified number if the zero padding is enabled,
/// otherwise does nothing.
#define AUDIO_ADD_PADDING(frames)	\
	(frames * ((AUDIO_ENABLE_PADDING) ? AUDIO_PADDING_RATIO : 1))

/// Converts the number of acquired frames to the number of values of
/// the corresponding magnitude-only half-complex FFT.
/// See FFTW documentation for further details.
#define AUDIO_FRAMES_TO_HALFCOMPLEX(frames) ((frames + 1) / 2 - 1)

/// Desired number of frames complete with padding
#define AUDIO_DESIRED_PADFRAMES	(AUDIO_ADD_PADDING(AUDIO_DESIRED_FRAMES))

/// Desired number of frames after being converted from halfcomplex notation to
/// magnitudes (comprehends padding frames if added)
#define AUDIO_DESIRED_HALFCOMPLEX	(AUDIO_FRAMES_TO_HALFCOMPLEX(AUDIO_DESIRED_PADFRAMES))

/// Desired dimension for the acquisition buffer
#define AUDIO_DESIRED_BUFFER_SIZE		(AUDIO_DESIRED_FRAMES)

/// Desired dimension for the FFT buffer, complete with padding if needed
#define AUDIO_DESIRED_PADBUFFER_SIZE	(AUDIO_DESIRED_PADFRAMES)

/// Desired dimension of the buffer that will contain
#define AUDIO_DESIRED_HALFBUFFER_SIZE	(AUDIO_DESIRED_HALFCOMPLEX)

/// Number of buffers used to record audio.
/// It is given as the count of tasks that can read/write recorded audio samples
/// plus one.
/// Such tasks are the microphone task and the gui task
#define AUDIO_REC_NUM_BUFFERS		(3)

/// Number of buffers used to publish FFTs.
/// It is given as the number of opened audio files (each potentially associated
/// with a task), plus the gui task, plus one
#define AUDIO_FFT_NUM_BUFFERS		(AUDIO_MAX_FILES+2)


/// Converts a certain number of frames in milliseconds, given current
/// capture rate
#define FRAMES_TO_MS(frames,rate) \
	((1000L * STATIC_CAST(long,frames)) / STATIC_CAST(long,rate))

/// The desired period of the audio acquisition task.
/// NOTICE that the actual period is truncated to the millisecond of course.
#define AUDIO_DESIRED_PERIOD \
	FRAMES_TO_MS(AUDIO_DESIRED_FRAMES/AUDIO_LATENCY_REDUCER, AUDIO_DESIRED_RATE)

// -----------------------------------------------------------------------------
//                           GRAPHIC CONSTANTS
// -----------------------------------------------------------------------------

//@}

/**
 * @name Color palette
 */
//@{


//-------------------------------------------------------------
// Colors
//-------------------------------------------------------------

#define COLOR_MODE		(32)		///< Default color mode

#define COLOR_WHITE		(0xFFFFFF)	///< White
#define COLOR_BKG		(0xF0F0F0)	///< Light gray background
#define COLOR_PRIM_DARK	(0x388E3C)	///< Dark primary color
#define COLOR_PRIM		(0x4CAF50)	///< Primary color
#define COLOR_PRIM_LIGH	(0xC8E6C9)	///< Light prmary color
#define COLOR_ACCENT	(0x8BC34A)	///< Accent color
#define COLOR_TEXT		(0xFFFFFF)	///< Text/icons over a primary or accent color
#define COLOR_TEXT_PRIM	(0x212121)	///< Primary text color, over a background
#define COLOR_TEXT_SECN	(0x757575)	///< Secondary text color, over a prim or accent
#define COLOR_DIVIDER	(0xBDBDBD)	///< Color used for dividers

//-------------------------------------------------------------
// Window dimensions
//-------------------------------------------------------------

//@}

/**
 * @name Window properties
 */
//@{


// NOTICE: MX/MY values are considered outside the box

#define WIN_X		(0)				///< Start of coordinates x
#define WIN_Y		(0)				///< Start of coordinates y
#define WIN_MX		(1280)			///< Window x resolution
#define WIN_MY		(672)			///< Window y resolution

#define WIN_WIDTH	(WIN_MX-WIN_X)	///< Window width
#define WIN_HEIGHT	(WIN_MY-WIN_Y)	///< Window height

#define PADDING		(12)
									///< Base value for padding that can be
									///< used by child elements

//-------------------------------------------------------------
// The Footer is actually just an orange rectangle
//-------------------------------------------------------------

//@}

/**
 * @name Footer properties
 */
//@{


#define FOOTER_WIDTH	(WIN_WIDTH)				///< Footer width
#define FOOTER_HEIGHT	(48)					///< Footer height
#define FOOTER_X		(WIN_X)					///< Footer position x
#define FOOTER_Y		(WIN_MY-FOOTER_HEIGHT)	///< Footer position y
#define FOOTER_MX		(WIN_MX)				///< Footer max x
#define FOOTER_MY		(WIN_MY)				///< Footer max y

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

	The side panel contains the list of all the files opened by the program.
*/

//@}

/**
 * @name Side panel properties
 */
//@{

#define SIDE_X				(WIN_MX * 7/10)		///< Side panel position x
#define SIDE_Y				(0)					///< Side panel position y
#define SIDE_MX				(WIN_MX)			///< Side panel max x
#define SIDE_MY				(FOOTER_Y)			///< Side panel max y
#define SIDE_WIDTH			(SIDE_MX-SIDE_X)	///< Side panel width
#define SIDE_HEIGHT			(SIDE_MY-SIDE_Y)	///< Side panel height

#define SIDE_NUM_ELEMENTS	(AUDIO_MAX_FILES)
												///< The maximum number of
												///< elements in the side panel

#define SIDE_ELEM_X			(SIDE_X)			///< Side panel element position x

#define SIDE_ELEM_WIDTH		(SIDE_WIDTH)		///< Side panel element width
#define SIDE_ELEM_HEIGHT	(SIDE_HEIGHT/SIDE_NUM_ELEMENTS)
												///< Side panel element height

#define SIDE_ELEM_MX		(SIDE_MX-SIDE_X)	///< Side panel element max x
#define SIDE_ELEM_MY		(SIDE_ELEM_X + SIDE_ELEM_HEIGHT)
												///< Side panel element max y

#define SIDE_ELEM_NAME_X	(SIDE_ELEM_X + 12)
											///< Side panel element name label
											///< absolute x (same for each element)
#define SIDE_ELEM_NAME_Y	(20)			///< Side panel element name label relative y

#define SIDE_ELEM_VOL_LABEL_X	(12)		///< Side panel element volume label relative x
#define SIDE_ELEM_VOL_LABEL_Y	(40)		///< Side panel element volume label relative y
#define SIDE_ELEM_PAN_LABEL_X	(12+128)	///< Side panel element panning label relative x
#define SIDE_ELEM_PAN_LABEL_Y	(40)		///< Side panel element panning label relative y
#define SIDE_ELEM_FRQ_LABEL_X	(12+256-8)	///< Side panel element frequency label relative x
#define SIDE_ELEM_FRQ_LABEL_Y	(40)		///< Side panel element frequency label relative y

#define SIDE_ELEM_VAL_Y			(54)		///< Side panel element text value y

#define SIDE_ELEM_VOL_X	(SIDE_ELEM_X + 64)		///< Side element volume value x
#define SIDE_ELEM_PAN_X	(SIDE_ELEM_X + 64+128)	///< Side element panning value x
#define SIDE_ELEM_FRQ_X	(SIDE_ELEM_X + 64+256)	///< Side element frequency value x

// BUTTONS, NOTICE: these values are relative to the elem position!

#define BUTTON_PLAY_X		(360)			///< Button play relative x
#define BUTTON_PLAY_Y		(10)			///< Button play relative y
#define BUTTON_PLAY_MX		(374)			///< Button play relative mx
#define BUTTON_PLAY_MY		(30)			///< Button play relative my

#define BUTTON_ROW_Y		(50)
											///< Buttons row position y
											///< Used in CHECK_BUTTON_POSX() and
											///< CHECK_BUTTON_POSY() macros
#define BUTTON_ROW_MY		(66)
											///< Buttons row max y
											///< Used in CHECK_BUTTON_POSX() and
											///< CHECK_BUTTON_POSY() macros

#define BUTTON_VOL_DOWN_X	(40)			///< Button volume down position x
#define BUTTON_VOL_DOWN_MX	(56)			///< Button volume down max x
#define BUTTON_PAN_DOWN_X	(40 + 128)		///< Button panning down position x
#define BUTTON_PAN_DOWN_MX	(56 + 128)		///< Button panning down max x
#define BUTTON_FRQ_DOWN_X	(40 + 256)		///< Button frequency down position x
#define BUTTON_FRQ_DOWN_MX	(56 + 256)		///< Button frequency down max x

#define BUTTON_VOL_UP_X		(102)			///< Button volume up position x
#define BUTTON_VOL_UP_MX	(118)			///< Button volume up max x
#define BUTTON_PAN_UP_X		(102 + 128)		///< Button panning up position x
#define BUTTON_PAN_UP_MX	(118 + 128)		///< Button panning up max x
#define BUTTON_FRQ_UP_X		(102 + 256)		///< Button frequency up position x
#define BUTTON_FRQ_UP_MX	(118 + 256)		///< Button frequency up max x

/// Concatenates two strings to form another macro.
#define PASTER(button, dim) BUTTON_ ## button ## _ ## dim

/// Checks whether the given position is inside the given button y boundaries.
/// Example usage:
/// \code
/// // To check if the button is on the same y position of the play button
/// CHECK_BUTTON_POSY(relative_mouse_y_pos, PLAY)
/// \endcode
#define CHECK_BUTTON_POSY(posy, button) \
	(posy >= PASTER(button,Y) && posy < PASTER(button,MY))

/// Checks whether the given position is inside the given button x boundaries.
/// Example usage:
/// \code
/// // To check if the button is on the same x position of the play button
/// CHECK_BUTTON_POSX(relative_mouse_x_pos, PLAY)
/// \endcode
#define CHECK_BUTTON_POSX(posx, button) \
	(posx >= PASTER(button,X) && posx < PASTER(button,MX))


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

	The FFT panel contains the FFT of the sound received from the recorder.
	The scale and background are contained in a bitmap file, thus they are
	static.
*/

//@}

/**
 * @name FFT panel properties
 */
//@{

#define FFT_P		(PADDING)				///< FFT panel padding
#define FFT_X		(WIN_X + FFT_P)			///< FFT panel position x
#define FFT_Y		(WIN_Y + FFT_P)			///< FFT panel position y
#define FFT_MX		(SIDE_X - FFT_P)		///< FFT panel max x
#define FFT_MY		(FOOTER_Y/2 - FFT_P)	///< FFT panel max y

#define FFT_PLOT_X	(FFT_X + FFT_P)			///< FFT plot position x
#define FFT_PLOT_Y	(FFT_Y + FFT_P)			///< FFT plot position y
#define FFT_PLOT_MX	(FFT_MX - 50)			///< FFT plot max x
#define FFT_PLOT_MY	(FFT_MY - 2 * FFT_P)	///< FFT plot max y

/// FFT plot width
#define FFT_PLOT_WIDTH	(FFT_PLOT_MX - FFT_PLOT_X)

/// FFT plot height
#define FFT_PLOT_HEIGHT	(FFT_PLOT_MY - FFT_PLOT_Y)

#define FFT_PLOT_X_TICKS	(10)	///< Number of ticks on the FFT plot X scale
#define FFT_PLOT_Y_TICKS	(5)		///< Number of ticks on the FFT plot Y scale

/// FFT panel X scale position y
#define FFT_PLOT_X_SCALE_Y			(FFT_PLOT_MY + 5)
/// FFT panel X scale position my (tick)
#define FFT_PLOT_X_SCALE_MY			(FFT_PLOT_X_SCALE_Y + 5)
/// FFT panel X scale label position y
#define FFT_PLOT_X_SCALE_LABEL_Y	(FFT_PLOT_X_SCALE_MY + 5)
/// FFT panel X scale unit position x
#define FFT_PLOT_X_SCALE_UNIT_X		(FFT_PLOT_MX + 30)
/// FFT panel X scale unit position y
#define FFT_PLOT_X_SCALE_UNIT_Y		(FFT_PLOT_X_SCALE_LABEL_Y)

/// FFT panel Y scale position mx (tick)
#define FFT_PLOT_Y_SCALE_MX			(FFT_PLOT_X - 5)
/// FFT panel Y scale position x
#define FFT_PLOT_Y_SCALE_X			(FFT_PLOT_Y_SCALE_MX - 5)

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

//@}

/**
 * @name Time panel properties
 */
//@{

#define TIME_P	(PADDING)				///< Time panel padding
#define TIME_X	(0)						///< Time panel position x
#define TIME_Y	(FOOTER_Y/2 + 2*TIME_P)	///< Time panel position y
#define TIME_MX	(SIDE_X)				///< Time panel max x
#define TIME_MY	(FOOTER_Y)				///< Time panel max y

#define TIME_PLOT_X		(TIME_X + 2*TIME_P)		///< Time plot position x
#define TIME_PLOT_Y		(TIME_Y + TIME_P)		///< Time plot position y
#define TIME_PLOT_MX	(TIME_MX - 50)			///< Time plot max x
#define TIME_PLOT_MY	(TIME_MY - 4*TIME_P)	///< Time plot max y

/// Time plot width
#define TIME_PLOT_WIDTH		(TIME_PLOT_MX - TIME_PLOT_X)
/// Time plot height
#define TIME_PLOT_HEIGHT	(TIME_PLOT_MY - TIME_PLOT_Y)
/// Time plot middle position
#define TIME_PLOT_MIDDLE	((TIME_PLOT_Y + TIME_PLOT_MY) / 2)

/// The speed at which the time plot moves (in pixels per frame)
#define TIME_SPEED			(4)
/// Number of frames the time plot should skip before printing the next one
#define TIME_SHOULD_SKIP	(1)
/// The actual speed of the plot
#define TIME_ACTUAL_SPEED	(TIME_SPEED / (TIME_SHOULD_SKIP+1))
/// The width of the bar, as a fraction of TIME_SPEED
#define TIME_FILL			(2)

/// The maximum height of the time plot content
#define TIME_MAX_HEIGHT		(TIME_PLOT_HEIGHT/2)

#define TIME_PLOT_X_TICKS	(10)	///< Number of ticks on the TIME plot X scale
#define TIME_PLOT_Y_TICKS	(4)		///< Number of ticks on the TIME plot Y scale

/// TIME panel X scale position y
#define TIME_PLOT_X_SCALE_Y				(TIME_PLOT_MY + 5)
/// TIME panel X scale position my (tick)
#define TIME_PLOT_X_SCALE_MY			(TIME_PLOT_X_SCALE_Y + 5)
/// TIME panel X scale label position y
#define TIME_PLOT_X_SCALE_LABEL_Y		(TIME_PLOT_X_SCALE_MY + 5)
/// TIME panel X scale unit position x
#define TIME_PLOT_X_SCALE_UNIT_X		(TIME_PLOT_MX + 20)
/// TIME panel X scale unit position y
#define TIME_PLOT_X_SCALE_UNIT_Y		(TIME_PLOT_X_SCALE_LABEL_Y)

/// TIME panel Y scale position mx (tick)
#define TIME_PLOT_Y_SCALE_MX			(TIME_PLOT_X - 5)
/// TIME panel Y scale position x
#define TIME_PLOT_Y_SCALE_X				(TIME_PLOT_Y_SCALE_MX - 5)





//-------------------------------------------------------------
// TASKS CONSTANTS
//-------------------------------------------------------------

//@}

/**
 * @name Tasks properties
 */
//@{

// The tasks are: gui, user interaction, microphone, checkdata and analysis.
#define TASK_GUI		(0)
#define TASK_UI			(1)
#define TASK_CHK		(2)
#define TASK_MIC		(2)
#define TASK_ALS_FIRST	(3)

/// Maximum number of tasks which may be running at any time
#define	TASK_NUM		(TASK_ALS_FIRST + AUDIO_MAX_FILES)

/// A zero wcet means that the value is unknown
#define WCET_UNKNOWN	(0)

#ifdef NDEBUG
/// If not in debug mode, this does nothing
#define GET_PRIO(prio) (prio)
#else
/// In debug mode real-time scheduling is disabled, so priority must be zero
#define GET_PRIO(prio) (0)
#endif

// Tasks priorities are chosen following RM guidelines, even if the UI task has
// a lower priority than the microphone tasks because the responsiveness to the
// microphone inputs has a much greater importance than responsiveness to
// keyboard/mouse inputs.

// GUI TASK

#define TASK_GUI_WCET		(WCET_UNKNOWN)
#define TASK_GUI_PERIOD		(16)	///< A little more than 60 fps
// #define TASK_GUI_PERIOD	(30)	///< A little more than 30 fps
#define TASK_GUI_DEADLINE	(TASK_GUI_PERIOD)
#define TASK_GUI_PRIORITY	(1)
									///< Lowest priority, missing a frame is not
									///< a big deal, system responsiveness is
									///< much more important

// USER INTERACTION TASK

#define TASK_UI_WCET		(WCET_UNKNOWN)
#define TASK_UI_PERIOD		(10)	///< A hundred times per second
#define TASK_UI_DEADLINE	(10)
#define TASK_UI_PRIORITY	(2)

// CHECK DATA TASK (this is used only if AUDIO_APERIODIC is defined)

#define TASK_CHK_WCET		(WCET_UNKNOWN)
#define TASK_CHK_PERIOD		(1)
#define TASK_CHK_DEADLINE	(TASK_CHK_PERIOD)
#define TASK_CHK_PRIORITY	(4)

// MICROPHONE TASK

// NOTICE: if AUDIO_APERIODIC is defined, then the period of the microphone task
// doesn't matter because it is governed by the chekdata task
#define TASK_MIC_WCET		(WCET_UNKNOWN)
#define TASK_MIC_PERIOD		(AUDIO_DESIRED_PERIOD)
#define TASK_MIC_DEADLINE	(TASK_MIC_PERIOD)
#define TASK_MIC_PRIORITY	(3)

// ANALYSIS TASK (which may me many)

#define TASK_ALS_WCET		(WCET_UNKNOWN)
#define TASK_ALS_PERIOD		(AUDIO_DESIRED_PERIOD)
#define TASK_ALS_DEADLINE	(TASK_ALS_PERIOD)
#define TASK_ALS_PRIORITY	(3)

//@}

#endif
