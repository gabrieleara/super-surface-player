#ifndef AUDIO_H
#define AUDIO_H

/* -----------------------------------------------------------------------------
 * PUBLIC DATA TYPES
 * -----------------------------------------------------------------------------
 */

// Opened file type
typedef enum __AUDIO_TYPE_ENUM
{
	AUDIO_TYPE_INVALID = -1,
	AUDIO_TYPE_SAMPLE,
	AUDIO_TYPE_MIDI,
} audio_type_t;


/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/* ------- UNSAFE FUNCTIONS - CALL ONLY IN SINGLE THREAD ENVIRONMENT -------- */

/*
 * Initializes the mutex TODO: and the other required data structures.
 */
extern int audio_init();

/*
 * Opens the file specified by the filename
 */
extern int audio_open_file(const char *filename);

/*
 * Closes an opened audio file and shifts all the indexes of opened audio files
 * back if needed.
 */
extern int audio_close_file(int filenum);

/*
 * List audio files on stdandard output.
 */
extern void audio_list_files();

/* ------------- SAFE FUNCTIONS - CAN BE CALLED FROM ANY THREAD ------------- */

/*
 * Plays the file specified by the number
 */
extern int audio_play_file(int filenum);

/*
 * Stops any audio or midi that is currently playing.
 */
extern void audio_stop();

/*
 * Getters functions
 */
extern int audio_get_num_files();

extern const char* audio_get_filename(int i);

extern int audio_get_volume(int i);

extern int audio_get_panning(int i);

extern int audio_get_frequency(int i);

extern audio_type_t audio_get_type(int i);

/*
 * Setters functions
 */
extern void audio_set_volume(int i, int val);

extern void audio_set_panning(int i, int val);

extern void audio_set_frequency(int i, int val);

/*
 * Modifiers functions
 */
extern void audio_volume_up(int i);

extern void audio_volume_down(int i);

extern void audio_panning_up(int i);

extern void audio_panning_down(int i);

extern void audio_frequency_up(int i);

extern void audio_frequency_down(int i);

/* -----------------------------------------------------------------------------
 * TASKS
 * -----------------------------------------------------------------------------
 */

extern void* microphone_task(void *arg);

#endif
