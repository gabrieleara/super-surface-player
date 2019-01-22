/**
 * @file audio.c
 * @brief Audio-related functions and data types
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * For public functions, documentation can be found in corresponding header file:
 * audio.h.
 *
 * Except the ones that shall be called from a single-thread environment,
 * functions are safe from a concurrency point of view.
 *
 */

// Standard libraries
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <libgen.h>			// Used for basename
#include <complex.h>		// Used for C99 standard complex numbers in fftw3

#include <assert.h>			// Used in debug

// Linked libraries
#include <allegro.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>

// Custom libraries
#include "api/std_emu.h"
#include "api/time_utils.h"
#include "api/ptask.h"

// Other modules
#include "constants.h"
#include "audio.h"
#include "main.h"

// -----------------------------------------------------------------------------
//                           PRIVATE CONSTANTS
// -----------------------------------------------------------------------------

#define AUDIO_INPUT_MONO	0	///< Input mode set to mono
#define AUDIO_INPUT_STEREO	1	///< Input mode set to stereo

#define MAX_VOL		255			///< Maximum volume
#define MIN_VOL		0			///< Minimum volume (silent)

#define CRX_PAN		255			///< Right channel panning
#define CLX_PAN		0			///< Left channel panning
#define MID_PAN		128			///< Both channels panning

#define SAME_FRQ	1000		///< Play the sample at the original frequency

#define MAX_FREQ	9990		///< Maximum frequency adjustment
#define MIN_FREQ	0			///< Minimum frequency adjustment

#define MAX_AUDIO_FILES 8		///< The maximum number of opened audio files

#define MAX_AUDIO_NAME_LENGTH 32
								///< The maximum length of the audio file
								///< base name


// -----------------------------------------------------------------------------
//                           PRIVATE DATA TYPES
// -----------------------------------------------------------------------------

/// Pointer to the opened file type
typedef union __AUDIO_POINTER_UNION
{
	SAMPLE* audio_p;		///< Pointer to the audio SAMPLE structure
	MIDI*	midi_p;			///< Pointer to the MIDI structure
	void*	gen_p;			///< Fallback generic pointer
} audio_pointer_t;


/// Opened audio file descriptor
typedef struct __AUDIO_FILE_DESC_STRUCT
{
	// NOTE: When modifying the audio_file_desc_t data structure, consider
	// which state should be adopted as default state.

	audio_pointer_t datap;		///< Pointer to the opened file
	audio_type_t	type;		///< File type
	int				volume;		///< Volume used when playing this file
	int				panning;	///< Panning used when playing this file
	int				frequency;	///< Frequency used when playing this file,
								///< 1000 is the base frequency

	// IDEA: I decided to not enable loop execution of audio files. If there is
	// time left consider implementing it.
	// bool loop;				///< Tells if the audio should be reproduced in a loop

	char filename[MAX_AUDIO_NAME_LENGTH];
								///< Name of the file displayed on the screen,
								///< contains only the basename, ellipsed if
								///< too long

	bool has_rec;				///< Tells if the file has an associated
								///< recorded audio that can be recognized to
								///< start it playing

	// TODO: How should I define an association between an audio file and the
	// audio record that should be used to associate it with another file?
} audio_file_desc_t;

/// Status of the resources used to record audio
typedef struct __AUDIO_RECORD_STRUCT
{
	unsigned int rrate;			///< Recording acquisition ratio, as accepted
								///< by the device

	snd_pcm_uframes_t rframes;	///< Period requested to recorder task,
								///< expressed in terms of number of frames

	snd_pcm_t *alsa_handle;		///< ALSA Hardware Handle used to record audio

	short buffers[AUDIO_NUM_BUFFERS][AUDIO_DESIRED_FRAMES];
								///< Buffers used within the cab

	ptask_cab_t cab;			///< CAB used to handle allocated buffers
} audio_record_t;

/// Status of the resources used to perform fft
typedef struct __AUDIO_FFT_STRUCT
{
	unsigned int rrate;			///< Recording acquisition ratio, as accepted
								///< by the device

	snd_pcm_uframes_t rframes;	///< Period requested to recorder task,
								///< expressed in terms of number of frames

	fftw_plan plan;				///< The plan used to perform the FFT

	double buffers[AUDIO_NUM_BUFFERS][AUDIO_DESIRED_FRAMES];
								///< Buffers used within the cab, their
								///< structure is the one of an
								///< Halfcomplex-formatted FFT

	ptask_cab_t cab;			///< CAB used to handle allocated buffers

} audio_fft_t;

/// Global state of the module
typedef struct __AUDIO_STRUCT
{
	// NOTE: When modifying the audio_state_t data structure, consider
	// which state should be adopted as default state.
	audio_file_desc_t audio_files[MAX_AUDIO_FILES];
								///< Array of opened audio files descriptors for
								///< reproduction

	int opened_audio_files;		///< Number of currently opened audio files

	audio_record_t record;		///< Contains all the data needed to record

	audio_fft_t fft;			///< Contains all the data needed to perform
								///< FFT analysis

	ptask_mutex_t mutex;		///< TODO: Right now accesses to this data
								///< structure are not protected (enough).
								///< A security check shall be performed to be
								///< sure that each new code does not collide
								///< with the old one.
} audio_state_t;


/// Base empty audio file descriptor
const audio_file_desc_t audio_file_new =
{
	.type		= AUDIO_TYPE_SAMPLE,
	.volume		= MAX_VOL,
	.panning	= MID_PAN,
	.frequency	= SAME_FRQ,
	.filename	= "",
	.has_rec	= false,
	// .loop		= false,
};

// -----------------------------------------------------------------------------
//                           GLOBAL VARIABLES
// -----------------------------------------------------------------------------

/// The variable keeping the whole state of the audio module
static audio_state_t audio_state =
{
	.opened_audio_files = 0,
};

// -----------------------------------------------------------------------------
//                           PRIVATE FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * @name Private functions
 */
//@{

/**
 * Copies the basename of the given path into dest, ellipsing it if too long.
 */
static inline void path_to_basename(char* dest, const char* path)
{
char	buffer[MAX_CHAR_BUFFER_SIZE];// Buffer, used to prevent path modifications
char	*base_name;
int		len;

	// The basename function may modify the source string, thus we need to copy
	// it into a temporary buffer
	strncpy(buffer, path, MAX_CHAR_BUFFER_SIZE);

	base_name = basename(buffer);

	// TODO: since the original buffer may be modified, I should check the
	// length on the original path, shouldn't I?
	// len = strnlen(buffer, MAX_CHAR_BUFFER_SIZE);
	len = strnlen(path, MAX_CHAR_BUFFER_SIZE);

	if (len >= MAX_AUDIO_NAME_LENGTH)
	{
		base_name[MAX_AUDIO_NAME_LENGTH - 4] =
			base_name[MAX_AUDIO_NAME_LENGTH - 3] =
			base_name[MAX_AUDIO_NAME_LENGTH - 2] = '.';

		base_name[MAX_AUDIO_NAME_LENGTH - 1] = '\0';
	}

	strncpy(dest, base_name, MAX_AUDIO_NAME_LENGTH);
}

/**
 * Initialize ALSA recorder handle.
 * TODO: document arguments
 */
int alsa_install_recorder(snd_pcm_t **alsa_handle_ptr,
	unsigned int *rrate_ptr, snd_pcm_uframes_t *rframes_ptr)
{
int err;

unsigned int rrate = *rrate_ptr;// Recording acquisition ratio, as accepted
								// by the device
snd_pcm_uframes_t rframes = *rframes_ptr;
								// Period requested to recorder task, expressed
								// in terms of number of frames

snd_pcm_t *alsa_handle;			// ALSA Hardware Handle used to record audio
snd_pcm_hw_params_t *hw_params; // Parameters used to configure ALSA hardware

	// Open PCM device for recording (capture)
	err = snd_pcm_open(&alsa_handle, "default",
					SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (err < 0) return err;

	// Allocate a hardware parameters object
	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0) return err;

	// Fill it with default values
	err = snd_pcm_hw_params_any(alsa_handle, hw_params);
	if (err < 0) return err;

	// From now on we set desired hardware parameters.

	// Interleaved mode (we'll use only one channel though)
	err = snd_pcm_hw_params_set_access(alsa_handle, hw_params,
									   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) return err;

	// Signed 16-bit little-endian format
	err = snd_pcm_hw_params_set_format(alsa_handle, hw_params,
									SND_PCM_FORMAT_S16_LE);
	if (err < 0) return err;

	// Settinhg the sampling rate.
	// After this call rrate contains the real rate at which the device will
	// record
	err = snd_pcm_hw_params_set_rate_near(alsa_handle, hw_params, &rrate, 0);
	if (err < 0) return err;

	// Setting one channel only (mono)
	err = snd_pcm_hw_params_set_channels(alsa_handle, hw_params, 1);
	if (err < 0) return err;

	// Setting execution period size based on number of frames of the buffer
	// After this call, rframes will contain the actual period accepted by the
	// device
	err = snd_pcm_hw_params_set_period_size_near(alsa_handle, hw_params,
												 &rframes, 0);
	if (err < 0) return err;

	// Writing parameters to the driver
	err = snd_pcm_hw_params(alsa_handle, hw_params);
	if (err < 0) return err;

	// Freeing up local resources allocated via malloc
	snd_pcm_hw_params_free(hw_params);

	// Copying into INOUT parameters
	*alsa_handle_ptr	= alsa_handle;
	*rrate_ptr			= rrate;
	*rframes_ptr		= rframes;

	return 0;
}

/**
 * Initializes both Allegro sound and ALSA library to record
 */
static inline int allegro_alsa_sound_init(unsigned int *rrate_ptr,
	snd_pcm_uframes_t *rframes_ptr, snd_pcm_t **alsa_handle_ptr)
{
int err;
int index;

void *cab_pointers[AUDIO_NUM_BUFFERS];
								// Pointers to buffers used in cab library

	// Allegro sound initialization
	// FIXME: MIDI files do not work, consider enabling MIDI_AUTODETECT and
	// implement MIDI files support
	err = install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL);
	if (err) return err;

	// Initialization of ALSA recorder
	err = alsa_install_recorder(alsa_handle_ptr, rrate_ptr, rframes_ptr);
	if (err) return err;

	// Construction of CAB pointers for audio buffers
	for (index = 0; index < AUDIO_NUM_BUFFERS; ++index)
	{
		cab_pointers[index] = STATIC_CAST(void*, audio_state.record.buffers[index]);
	}

	// Initializing capture CAB buffers, the pointers are copied to the cab
	// structure
	err = ptask_cab_init(&audio_state.record.cab,
		AUDIO_NUM_BUFFERS,
		AUDIO_DESIRED_FRAMES,
		cab_pointers);

	return err;
}

/**
 * Initializes FFTW3 library to perform FFT analysis
 */
static inline int fft_init(snd_pcm_uframes_t rframes, fftw_plan* fft_plan_ptr)
{
int err;
int index;

void *cab_pointers[AUDIO_NUM_BUFFERS];
								// Pointers to buffers used in cab library

double *inout;					// Array that will be used to create a
								// FFTW3 plan

	// The array is allocated dynamically, but it will be deallocated as soon a
	// the construction of the plan is finished. It's the only thing that is
	// allocated dynamically.
	inout = STATIC_CAST(double*, fftw_malloc(sizeof(double) * rframes));

	// The returned plan is guaranteed not to be NULL
	*fft_plan_ptr = fftw_plan_r2r_1d(rframes,
						 inout,
						 inout,
						 FFTW_R2HC,
						 FFTW_EXHAUSTIVE); // OR FFTW_PATIENT

	// TODO: Implement wisdom saving/retrieval

	// From now on we can calculate the FFT using the given plan, by calling the
	// following function:
	// fftw_execute_r2r(const fftw_plan p, double *in, double *out);

	// Deallocation of the array
	fftw_free(inout);

	// Construction of CAB pointers for fft buffers
	for (index = 0; index < AUDIO_NUM_BUFFERS; ++index)
	{
		cab_pointers[index] = STATIC_CAST(void*, audio_state.fft.buffers[index]);
	}

	// Initializing FFT CAB buffers, the pointers are copied to the cab
	// structure
	err = ptask_cab_init(&audio_state.fft.cab,
						 AUDIO_NUM_BUFFERS,
						 AUDIO_DESIRED_FRAMES,
						 cab_pointers);

	return err;
}

/**
 * Prepares the microphone to record. Returns 0 on success, less than zero on
 * error.
 * TODO: check what are the return values.
 */
static inline int mic_prepare()
{
	return snd_pcm_prepare(audio_state.record.alsa_handle);
}

/**
 * Reads microphone data if available, microphone is non-blocking so this call
 * returns immediately with the number of read frames. Returns 0 if there were
 * no frame to read, less than zero on error.
*/
static inline int mic_read(short* buffer, const int nframes)
{
int err;

	err = snd_pcm_readi(audio_state.record.alsa_handle,
						STATIC_CAST(void *, buffer),
						nframes);

	// NOTE: This assumes no recovery code is available to be executed from
	// erroneus states. See mic_read_blocking.
#ifdef NDEBUG
	if (err < 0 && err != -EAGAIN)
		abort_on_error("Bad ALSA read!");
#endif

	return err;
}

/**
 * Waits for a specified amount of ms. It may wait for less time if the thread
 * is interrupted from outside.
 */
static inline void timed_wait(int ms)
{
	if (ms < 1)
		return;

	const struct timespec req = {
		.tv_sec = 0,
		.tv_nsec = ms * 1000000L,
	};

	while(clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL))
		;
}

/**
 * Reads microphone data if available, blocking until the number of frames that
 * is requested is not available yet.
 * TODO: add more to this documentation if the function is actually used.
 * TODO: actually probably this function will be used while in single-thread text mode.
*/
/*
static inline void mic_read_blocking(short* buffer, const int nframes)
{
int err;
int how_many_read = 0;	// How many frames have already been read
int missing = nframes;	// How many frames are still missing

	while (missing > 0)
	{
		err = mic_read(buffer + how_many_read, missing);

		if (err < 0)
		{
			switch (err)
			{
			case -EBADFD:
#ifdef NDEBUG
				abort_on_error("ALSA device was not in correct state.");
#else
				fprintf(stderr,
						"ALSA device was not in correct state: %s.\r\n",
						snd_strerror(err));
				assert(false);
#endif
				break;
			case -EPIPE:
#ifdef NDEBUG
				abort_on_error("Overrun in ALSA microphone handling.");
#else
				// NOTE: Could implement a recovery code from this state.
				fprintf(stderr, "ALSA overrun: %s.\r\n", snd_strerror(err));
				assert(false);
#endif
				break;
			case -ESTRPIPE:
#ifdef NDEBUG
				abort_on_error("ALSA suspend event occurred.");
#else
				// NOTE: Could implement a recovery code from this state.
				fprintf(stderr, "WARN: ALSA suspend event: %s.\r\n", snd_strerror(err));
				assert(false);
#endif
				break;
			case -EAGAIN:
				// No big deal, we can suspend for some time
				break;
			default:
#ifndef NDEBUG
				abort_on_error("ALSA unexpected error in blocking recording!");
#else
				fprintf(stderr, "ALSA unexpected error: %s.\r\n", snd_strerror(err));
				assert(false);
#endif
				break;
			}

#ifndef NDEBUG
			printf("LOG: missing=%d, delay=%ld (ms).\r\n", missing,
				FRAMES_TO_MS(missing, audio_state.record.rrate));
#endif

			timed_wait(FRAMES_TO_MS(missing, audio_state.record.rrate));
		}
		else
		{
			how_many_read	+= err;
			missing			-= err;
		}
	}
}
*/

//@}

// -----------------------------------------------------------------------------
//                           PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------

// See the header file for documentation

/* ------- UNSAFE FUNCTIONS - CALL ONLY IN SINGLE THREAD ENVIRONMENT -------- */

int audio_init()
{
int err;

unsigned int 		rrate	= AUDIO_DESIRED_RATE;
								// Recording acquisition ratio, as accepted by
								// the device

snd_pcm_uframes_t	rframes	= AUDIO_DESIRED_FRAMES;
								// Period requested to recorder task, expressed
								// in terms of number of frames

snd_pcm_t *alsa_handle;			// ALSA Hardware Handle used to record audio

fftw_plan fft_plan;				// The FFTW3 plan, which is the algorithm that will
								// be used to calculate the FFT, optimized for
								// the size of the recording buffer

	// Mutex initialization
	err = ptask_mutex_init(&audio_state.mutex);
	if (err) return err;

	// Allegro and ALSA initialization
	err = allegro_alsa_sound_init(&rrate, &rframes, &alsa_handle);
	if (err) return err;

	// FFTW3 initialization
	err = fft_init(rframes, &fft_plan);
	if (err) return err;

	// Copy local vales to global structures
	audio_state.record.rrate		= rrate;
	audio_state.record.rframes		= rframes;
	audio_state.record.alsa_handle	= alsa_handle;

	audio_state.fft.rrate			= rrate;
	audio_state.fft.rframes			= rframes;
	audio_state.fft.plan			= fft_plan;

	return 0;
}

int audio_open_file(const char *filename)
{
audio_pointer_t	file_pointer;	// Pointer to the opened file
audio_type_t	file_type;		// Detected file type
int				index;			// Index of newly used audio file descriptor

	if (audio_state.opened_audio_files == MAX_AUDIO_FILES)
		return EAGAIN;

	index = audio_state.opened_audio_files;

	file_pointer.audio_p = load_sample(filename);
	file_type = AUDIO_TYPE_SAMPLE;

	if (!file_pointer.gen_p)
	{
		// It was not an audio file, let's try if it was a MIDI istead
		file_pointer.midi_p = load_midi(filename);
		file_type = AUDIO_TYPE_MIDI;
	}

	if (file_pointer.gen_p)
	{
		// Valid input file

		audio_state.audio_files[index] = audio_file_new;

		audio_state.audio_files[index].type = file_type;

		path_to_basename(audio_state.audio_files[index].filename, filename);

		audio_state.audio_files[index].datap = file_pointer;

		++audio_state.opened_audio_files;
	}

	// If the pointer is NULL then the file was an invalid input
	return file_pointer.gen_p ? 0 : EINVAL;
}

int audio_close_file(int filenum)
{
int err = 0;

	if (filenum >= 0 && filenum < audio_state.opened_audio_files)
	{
		switch (audio_state.audio_files[filenum].type)
		{
		case AUDIO_TYPE_SAMPLE:
			destroy_sample(audio_state.audio_files[filenum].datap.audio_p);
			break;
		case AUDIO_TYPE_MIDI:
			destroy_midi(audio_state.audio_files[filenum].datap.midi_p);
			break;
		default:
			assert(false);
		}

		// NOTE: Depending on how files are associated with recorded trigger
		// samples, handle the removal of such triggers as well

		--audio_state.opened_audio_files;

		// Shift back array, starting from position of filenum
		for(; filenum < audio_state.opened_audio_files; ++filenum)
			audio_state.audio_files[filenum] = audio_state.audio_files[filenum+1];

		// audio_state.audio_files[audio_state.opened_audio_files] = audio_file_new;
	}
	else
		err = EINVAL;

	return err;
}

void audio_list_files()
{
int i;

	if (!audio_state.opened_audio_files)
	{
		printf("No audio files are opened.\r\n");
	}
	else
	{
		for(i = 0; i < audio_state.opened_audio_files; ++i)
		{
			printf("%d. %s\r\n", i+1, audio_state.audio_files[i].filename);
		}
	}
}

/* ------------- SAFE FUNCTIONS - CAN BE CALLED FROM ANY THREAD ------------- */

int audio_play_file(int filenum)
{
int					err = 0;
audio_file_desc_t	file;

	// Nobody can modify in multithreaded environment the number of opened audio
	// files
	if (filenum >= 0 && filenum < audio_state.opened_audio_files)
	{
		ptask_mutex_lock(&audio_state.mutex);

		file = audio_state.audio_files[filenum];

		ptask_mutex_unlock(&audio_state.mutex);

		switch (file.type)
		{
		case AUDIO_TYPE_SAMPLE:
			err = play_sample(
				file.datap.audio_p,
				file.volume,
				file.panning,
				file.frequency,
				false /*file.loop*/
				);

			err = err < 0 ? EINVAL : 0;

			break;
		case AUDIO_TYPE_MIDI:
			err = play_midi(
				file.datap.midi_p,
				false /*file.loop*/
				);

			break;
		default:
			assert(false);
			err = EINVAL;
		}
	}
	else
		err = EINVAL;

	return err;
}

void audio_stop()
{
int i;
	ptask_mutex_lock(&audio_state.mutex);

	for (i = 0; i < audio_state.opened_audio_files; ++i)
	{
		if (audio_state.audio_files[i].type == AUDIO_TYPE_SAMPLE)
			stop_sample(audio_state.audio_files[i].datap.audio_p);
	}

	ptask_mutex_unlock(&audio_state.mutex);

	stop_midi();
}

// -------------- GETTERS --------------

int audio_get_num_files()
{
	// Safe since the number of opened files cannot be modified in multithreaded
	// environment
	return audio_state.opened_audio_files;
}

const char* audio_get_filename(int i)
{
char* ret;

	if (i >= audio_state.opened_audio_files)
		return NULL;

	ptask_mutex_lock(&audio_state.mutex);

	ret = audio_state.audio_files[i].filename;

	ptask_mutex_unlock(&audio_state.mutex);

	return ret;
}

int audio_get_volume(int i)
{
int ret;

	if (i >= audio_state.opened_audio_files)
		return -1;

	ptask_mutex_lock(&audio_state.mutex);

	ret = audio_state.audio_files[i].volume;

	ptask_mutex_unlock(&audio_state.mutex);

	return ret;
}

int audio_get_panning(int i)
{
int ret;

	if (i >= audio_state.opened_audio_files)
		return -1;

	ptask_mutex_lock(&audio_state.mutex);

	ret = audio_state.audio_files[i].panning;

	ptask_mutex_unlock(&audio_state.mutex);

	return ret;
}

int audio_get_frequency(int i)
{
int ret;

	if (i >= audio_state.opened_audio_files)
		return -1;

	ptask_mutex_lock(&audio_state.mutex);

	ret = audio_state.audio_files[i].frequency / 10;

	ptask_mutex_unlock(&audio_state.mutex);

	return ret;
}

audio_type_t audio_get_type(int i)
{
	// Safe since the audio type cannot  be modified in multithreaded
	// environment

	if (i >= audio_state.opened_audio_files)
		return AUDIO_TYPE_INVALID;
	else
		return audio_state.audio_files[i].type;
}

// -------------- SETTERS --------------

void audio_set_volume(int i, int val)
{
	val = val < MIN_VOL ? MIN_VOL : val > MAX_VOL ? MAX_VOL : val;

	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	audio_state.audio_files[i].volume = val;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_set_panning(int i, int val)
{

	val = val < CLX_PAN ? CLX_PAN : val > CRX_PAN ? CRX_PAN : val;

	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	audio_state.audio_files[i].panning = val;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_set_frequency(int i, int val)
{
	val *= 10;

	val = val < MIN_FREQ ? MIN_FREQ : val > MAX_FREQ ? MAX_FREQ : val;

	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	audio_state.audio_files[i].frequency = val;

	ptask_mutex_unlock(&audio_state.mutex);

}

// -------------- MODIFIERS --------------

void audio_volume_up(int i)
{
	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	++audio_state.audio_files[i].volume;

	if (audio_state.audio_files[i].volume > MAX_VOL)
		audio_state.audio_files[i].volume = MAX_VOL;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_volume_down(int i)
{
	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	--audio_state.audio_files[i].volume;

	if (audio_state.audio_files[i].volume < MIN_VOL)
		audio_state.audio_files[i].volume = MIN_VOL;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_panning_up(int i)
{
	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	++audio_state.audio_files[i].panning;

	if (audio_state.audio_files[i].panning > CRX_PAN)
		audio_state.audio_files[i].panning = CRX_PAN;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_panning_down(int i)
{
	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	--audio_state.audio_files[i].panning;

	if (audio_state.audio_files[i].panning < CLX_PAN)
		audio_state.audio_files[i].panning = CLX_PAN;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_frequency_up(int i)
{
	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	audio_state.audio_files[i].frequency += 10;

	if (audio_state.audio_files[i].frequency > MAX_FREQ)
		audio_state.audio_files[i].frequency = MAX_FREQ;

	ptask_mutex_unlock(&audio_state.mutex);
}

void audio_frequency_down(int i)
{
	if (i >= audio_state.opened_audio_files)
		return;

	ptask_mutex_lock(&audio_state.mutex);

	audio_state.audio_files[i].frequency -= 10;

	if (audio_state.audio_files[i].frequency < MIN_FREQ)
		audio_state.audio_files[i].frequency = MIN_FREQ;

	ptask_mutex_unlock(&audio_state.mutex);
}


int audio_get_last_record(short* buffer_ptr[], int* buffer_index_ptr)
{
int err;

	err = ptask_cab_getmes(&audio_state.record.cab,
		STATIC_CAST(void**, buffer_ptr),
		buffer_index_ptr,
		NULL);

	if (err)
		return -EAGAIN;

	return audio_state.record.rframes;
}

void audio_free_last_record(int buffer_index)
{
	ptask_cab_unget(&audio_state.record.cab, buffer_index);
}

int audio_get_last_fft(double *buffer_ptr[], int *buffer_index_ptr)
{
	int err;

	err = ptask_cab_getmes(&audio_state.fft.cab,
						   STATIC_CAST(void **, buffer_ptr),
						   buffer_index_ptr,
						   NULL);

	if (err)
		return -EAGAIN;

	return audio_state.fft.rframes;
}

void audio_free_last_fft(int buffer_index)
{
	ptask_cab_unget(&audio_state.fft.cab, buffer_index);
}

int audio_get_rrate() {
	return audio_state.fft.rrate;
}

int audio_get_rframes() {
	return audio_state.fft.rframes;
}

// -----------------------------------------------------------------------------
//                                  TASKS
// -----------------------------------------------------------------------------

/// The body of the microphone task
void* microphone_task(void *arg)
{
ptask_t*	tp;			// task pointer
// int			task_id;	// task identifier

// Local variables
int		err;

int 	buffer_index;	// Index of local buffer used to get microphone data
						// used to access cab
short	*buffer;		// Pointer to local buffer, changes each time the buffer
						// is full
int		how_many_read;	// How many frames are already in the buffer
int		missing;		// How many frames are missing

	tp = STATIC_CAST(ptask_t *, arg);
	// task_id = ptask_get_id(tp);

	// Variables initialization and initial computation

	// Preparing the microphone interface to be used
	err = mic_prepare();
	if (err)
		abort_on_error("Could not prepare microphone acquisition.");

	ptask_start_period(tp);

	// Get a local buffer from the CAB
	// There is no check because it never fails if used correcly
	ptask_cab_reserve(&audio_state.record.cab,
					  STATIC_CAST(void *, &buffer),
					  &buffer_index);
	how_many_read	= 0;
	missing			= audio_state.record.rframes;

	while (!main_get_tasks_terminate())
	{
		// While there is new data, keep capturing.
		// NOTICE: This is NOT an infinite loop, because the code is many times
		// faster than I/O.
		// NOTE: Change with an maximum time limit if this is a problem.
		while ((err = mic_read(buffer + how_many_read, missing)) > 0) {
			how_many_read	+= err;
			missing			-= err;

			if (missing == 0)
			{
				// Update most recent acquisition and request a new CAB

				// Release CAB to apply changes, a timestamp will be added to
				// the new data
				ptask_cab_putmes(&audio_state.record.cab, buffer_index);

				// Get a local buffer from the CAB
				// There is no check because it never fails if used correcly
				ptask_cab_reserve(&audio_state.record.cab,
								  STATIC_CAST(void**, &buffer),
								  &buffer_index);
				how_many_read	= 0;
				missing			= audio_state.record.rframes;
			}
		}

		if (ptask_deadline_miss(tp))
			printf("TASK_MIC missed %d deadlines!\r\n", ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup
	// Releasing unused half-empty buffer, this is needed because the reset does
	// not release any buffer that was previously reserved
	ptask_cab_unget(&audio_state.record.cab, buffer_index);

	// The reset can be done here because the only task that reserves buffers
	// for writing purposes is this one. If other threads are using buffers for
	// reading purposes, eventually they will unget them.
	ptask_cab_reset(&audio_state.record.cab);

	return NULL;
}

/// The body of the fft task
void *fft_task(void *arg)
{
ptask_t *tp; // task pointer
// int task_id; // task identifier

// Local variables
int err;

struct timespec last_timestamp = { .tv_sec = 0, .tv_nsec = 0};
						// Timestamp of last accessed input buffer
struct timespec new_timestamp;
						// Timestamp of the new input buffer

int in_buffer_index;	// Index of local buffer used to get microphone data
short *in_buffer;		// Pointer to local input buffer

int out_buffer_index;	// Index of local buffer used to produce fft data
double *out_buffer;		// Pointer to local output buffer

unsigned int i;			// Index used to copy data

	tp = STATIC_CAST(ptask_t *, arg);
	// task_id = ptask_get_id(tp);

	// Variables initialization and initial computation

	ptask_start_period(tp);

	while (!main_get_tasks_terminate())
	{
		err = ptask_cab_getmes(&audio_state.record.cab,
						STATIC_CAST(void **, &in_buffer),
						&in_buffer_index,
						&new_timestamp);

		// If the acquired buffer has not been analyzed yet
		if (err != EAGAIN && time_cmp(last_timestamp, new_timestamp) < 0)
		{
			last_timestamp = new_timestamp;

			// Get buffer on which operate
			ptask_cab_reserve(&audio_state.fft.cab,
							STATIC_CAST(void **, &out_buffer),
							&out_buffer_index);

			// Copy data onto new buffer
			for (i = 0; i < audio_state.fft.rframes; ++i)
			{
				out_buffer[i] = (double)in_buffer[i];
			}

			// Calculate in-place FFT using the plan computed above on
			// current buffer
			fftw_execute_r2r(audio_state.fft.plan, out_buffer, out_buffer);

			// NOTE: Output of previous FFT is an halfcomplex notation of the
			// FFT, it should be changed to a full array of magniutes to be
			// printed.

			// Publish new FFT
			ptask_cab_putmes(&audio_state.fft.cab, out_buffer_index);
		}
		// Otherwise, do nothing

		// Realease acquired buffer
		if (err == 0)
			ptask_cab_unget(&audio_state.record.cab, in_buffer_index);

		if (ptask_deadline_miss(tp))
			printf("TASK_FFT missed %d deadlines!\r\n", ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup
	// Releasing unused half-empty buffer, this is needed because the reset does
	// not release any buffer that was previously reserved
	// TODO: why this cleanup call is commented out?
	//ptask_cab_unget(&audio_state.record.cab, buffer_index);

	// The reset can be done here because the only task that reserves buffers
	// for writing purposes is this one
	ptask_cab_reset(&audio_state.fft.cab);

	return NULL;
}
