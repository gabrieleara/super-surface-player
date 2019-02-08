/**
 * @file audio.h
 * @brief Audio-related public functions and data types
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * This module handles all interactions between the program and audio services
 * provided either by Allegro or directly via the ALSA library, both for
 * recording and playback purposes.
 * It also uses FFTW library to compute FFT/IFFT of the acquired audio samples
 * and publishes results on CAB structures that can be accessed via getters.
 *
 * Except the ones that shall be called from a single-thread environment,
 * functions are safe from a concurrency point of view.
 *
 */

#ifndef AUDIO_H
#define AUDIO_H

// -----------------------------------------------------------------------------
//                             PUBLIC DATA TYPES
// -----------------------------------------------------------------------------

/**
 * The state associated with an audio entry (since they are statically
 * allocated).
 */
typedef enum __AUDIO_TYPE_ENUM
{
	AUDIO_TYPE_INVALID = -1,	///< Invalid audio file entry
	AUDIO_TYPE_SAMPLE,			///< Sample-based audio file entry (wav)
	AUDIO_TYPE_MIDI,			///< MIDI audio file entry
} audio_type_t;

// -----------------------------------------------------------------------------
//                             PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------

/* ------- UNSAFE FUNCTIONS - CALL ONLY IN SINGLE THREAD ENVIRONMENT -------- */

/**
 * @name Functions that shall be called in a single thread environment
 */
//@{

/**
 * Initializes the audio module.
 */
extern int audio_init();

/**
 * Opens the file specified by the filename.
 * The filename shall be the complete absolute path of the file.
 */
extern int audio_file_open(const char *filename);

/**
 * Returns true if the specified audio file is open.
 */
extern bool audio_file_is_open(int i);

/**
 * Closes an opened audio file and shifts all the indexes of opened audio files
 * back if needed.
 */
extern int audio_file_close(int filenum);

/**
 * Displays a countdown and then records an audio sample that can be used to
 * trigger the specified audio file.
 * Returns zero on success.
 */
extern int audio_file_record_sample_to_play(int i);

/**
 * PLays the recorded audio sample associated with the specified file.
 */
extern void audio_file_play_recorded_sample(int i);

/**
 * Discards the recorded audio sample associated with the specified file.
 */
extern void audio_file_discard_recorded_sample(int i);


//@}

/* ------------- SAFE FUNCTIONS - CAN BE CALLED FROM ANY THREAD ------------- */

/**
 * @name Play/Pause functions
 */
//@{

/**
 * Plays the file specified by the number
 */
extern int audio_file_play(int filenum);

/**
 * Stops any audio or midi that is currently playing.
 * Only all audio at once can be stopped, there is no way to stop a specific
 * file only.
 */
extern void audio_stop();

//@}

/**
 * @name Getters
 */
//@{

/// Returns the number of opened audio files.
extern int audio_file_num_opened();

/**
 * Returns true if the given file has an associated recording.
 * WARNING: no check whether the given audio file index if performed.
 */
extern bool audio_file_has_rec(int i);

/**
 * Returns a null terminated string containing the file name.
 * WARNING: no check whether the given audio file index if performed.
 */
extern const char* audio_file_name(int i);


/// Returns the volume associated with the file corresponding to the given index
extern int audio_file_get_volume(int i);

/// Returns the paning associated with the file corresponding to the given index
extern int audio_file_get_panning(int i);

/// Returns the frequency adjustment associated with the file corresponding to
/// the given index
extern int audio_file_get_frequency(int i);

/// Returns whether the file associated with the given index is an audio file,
/// a MIDI file or an invalid file entry.
extern audio_type_t audio_file_type(int i);

//@}

/**
 * @name Setters
 */
//@{

/// Changes the volume associated with the file corresponding to the given index
extern void audio_file_set_volume(int i, int val);

/// Changes the panning associated with the file corresponding to the given index
extern void audio_file_set_panning(int i, int val);

/// Changes the frequency adjustment associated with the file corresponding to
/// the given index
extern void audio_file_set_frequency(int i, int val);

//@}

/**
 * @name Functions that modify global properties
 */
//@{

/// Changes the volume associated with the file corresponding to the given index,
/// increasing it by one value
extern void audio_file_volume_up(int i);

/// Changes the volume associated with the file corresponding to the given index,
/// decreasing it by one value
extern void audio_file_volume_down(int i);

/// Changes the panning associated with the file corresponding to the given
/// index, increasing it by one value
extern void audio_file_panning_up(int i);

/// Changes the panning associated with the file corresponding to the given
/// index, decreasing it by one value
extern void audio_file_panning_down(int i);

/// Changes the frequency adjustment associated with the file corresponding to
/// the given index, increasing it by one value
extern void audio_file_frequency_up(int i);

/// Changes the frequency adjustment associated with the file corresponding to
/// the given index, decreasing it by one value
extern void audio_file_frequency_down(int i);

//@}

/**
 * @name Recording-related functions
 */
//@{

/**
 * Fetches the most recent buffer acquired by the microphone using the CAB.
 * Returns its dimension or -EAGAIN if no data is available.
 */
extern int audio_get_last_record(const short *buffer_ptr[], int *buffer_index_ptr);

/**
 * Frees a the previously acquired audio buffer.
 */
extern void audio_free_last_record(int buffer_index);

/**
 * Fetches the most recent FFT buffer produced by the microphone task using the
 * CAB.
 * Returns its dimension or -EAGAIN if no data is available.
 */
extern int audio_get_last_fft(const double *buffer_ptr[], int *buffer_index_ptr);

/**
 * Frees a previously acquired fft buffer.
 */
extern void audio_free_last_fft(int buffer_index);

/**
 * Returns the real acquisition rate of the recorder.
 */
extern int audio_get_record_rrate();

/**
 * Returns the number of frames captured by the recorder for each sample.
 */
extern int audio_get_record_rframes();

/**
 * Returns the acquisition rate of the recorder, which is also the frequency
 * that is considered as a base when calculating the FFT of a signal.
 */
extern int audio_get_fft_rrate();

/**
 * Returns the number of frames (complete with padding if needed) that are used
 * when calculating the FFT, which is also the dimension of the FFT output.
 */
extern int audio_get_fft_rframes();

//@}

// -----------------------------------------------------------------------------
//                                  TASKS
// -----------------------------------------------------------------------------


/**
 * @name Recording-related Tasks
 */
//@{

/// The body of the checkdata task
extern void* checkdata_task(void *arg);

/// The body of the microphone task
extern void *microphone_task(void *arg);

/// The body of the analyzer task
extern void *analysis_task(void *arg);

//@}

#endif
