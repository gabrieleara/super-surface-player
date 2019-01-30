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
#include <math.h>
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

#define MAX_AUDIO_NAME_LENGTH 32
								///< The maximum length of the audio file
								///< base name

#define COUNTDOWN_SECONDS	5
								///< The number of seconds to wait before
								///< recording an audio sample

// HACK: move somewhere else
#define IS_ODD(n) (n & 0x01)
#define IS_EVEN(n) (!IS_ODD(n))

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
	// I decided to not enable loop execution of audio files.
	// bool loop;				///< Tells if the audio should be reproduced in a loop

	bool has_rec;				///< Tells if the file has an associated
								///< recorded audio that can be recognized to
								///< start it playing

	char filename[MAX_AUDIO_NAME_LENGTH];
								///< Name of the file displayed on the screen,
								///< contains only the basename, ellipsed if
								///< too long

	double autocorr;			///< The cross correlation of the signal with itself

	short recorded_sample[AUDIO_DESIRED_BUFFER_SIZE];
								///< Contains the recorded sample by the user
								///< to play the audio file whenever a sample
								///< similar to the recorded one is detected

	double recorded_fft[AUDIO_DESIRED_PADBUFFER_SIZE];
								///< Contains the FFT of the recorded sample,
								///< precomputed for efficiency reasons
} audio_file_desc_t;

/// Status of the resources used to record audio
typedef struct __AUDIO_RECORD_STRUCT
{
	unsigned int rrate;			///< Recording acquisition ratio, as accepted
								///< by the device

	snd_pcm_uframes_t rframes;	///< Period requested to recorder task,
								///< expressed in terms of number of frames

	snd_pcm_t *record_handle;	///< ALSA Hardware Handle used to record audio
	snd_pcm_t *playback_handle;	///< ALSA Hardware Handle used to playback
								///< recorded audio

	short buffers[AUDIO_NUM_BUFFERS][AUDIO_DESIRED_BUFFER_SIZE];
								///< Buffers used within the cab

	ptask_cab_t cab;			///< CAB used to handle allocated buffers
} audio_record_t;

typedef struct __FFT_OUTPUT
{
	double autocorr;			///< The autocorrelation of the given sample
	double fft[AUDIO_DESIRED_PADBUFFER_SIZE];
								///< The FFT of the given sample, its format is
								///< Halfcomplex-formatted FFT
} fft_output_t;

/// Status of the resources used to perform fft
typedef struct __AUDIO_FFT_STRUCT
{
	unsigned int rrate;			///< Recording acquisition ratio, as accepted
								///< by the device

	snd_pcm_uframes_t rframes;	///< Period requested to recorder task,
								///< expressed in terms of number of frames

	fftw_plan plan;				///< The plan used to perform the FFT
	fftw_plan plan_inverse;		///< The plan used to perform the inverse FFT

	fft_output_t buffers[AUDIO_NUM_BUFFERS];
								///< Buffers used within the cab

	ptask_cab_t cab;			///< CAB used to handle allocated buffers

} audio_fft_t;

/// Structure that contains a cab used by the unnormalized_cross_correlation()
/// function
typedef struct __AUDIO_ANALYSIS_STRUCT
{
	double buffers[AUDIO_MAX_FILES][AUDIO_DESIRED_PADBUFFER_SIZE];
								///< Buffers used within the cab
	ptask_cab_t cab;			///< The CAB is used as a buffer pool
} audio_analysis_t;

/// Global state of the module
typedef struct __AUDIO_STRUCT
{
	// NOTE: When modifying the audio_state_t data structure, consider
	// which state should be adopted as default state.
	audio_file_desc_t audio_files[AUDIO_MAX_FILES];
								///< Array of opened audio files descriptors for
								///< reproduction

	int opened_audio_files;		///< Number of currently opened audio files

	audio_record_t record;		///< Contains all the data needed to record

	audio_fft_t fft;			///< Contains all the data needed to perform
								///< FFT and IFFT

	audio_analysis_t analysis;	///< Contains all the data needed to perform
								///< analysis of FFTs

	ptask_mutex_t mutex;		///< TODO: Right now accesses to this data
								///< structure are not protected (enough).
								///< A security check shall be performed to be
								///< sure that each new code does not collide
								///< with the old one.
} audio_state_t;


/// Base empty audio file descriptor
const audio_file_desc_t audio_file_new =
{
	.datap		= { .gen_p = NULL },
	.type		= AUDIO_TYPE_SAMPLE,
	.volume		= MAX_VOL,
	.panning	= MID_PAN,
	.frequency	= SAME_FRQ,
	.has_rec	= false,
	// .loop		= false,
	.filename	= "",
};

void audio_file_copy(audio_file_desc_t *dest, const audio_file_desc_t*src)
{
	if (src->has_rec)
	{
		*dest = *src;
	}
	else
	{
		// I can skip copying the big arrays
		dest->datap		= src->datap;
		dest->type		= src->type;
		dest->volume	= src->volume;
		dest->panning	= src->panning;
		dest->frequency	= src->frequency;
		dest->has_rec	= src->has_rec;
		strcpy(dest->filename, src->filename);
	}
}

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

	// Get the length of the basename
	len = strnlen(base_name, MAX_CHAR_BUFFER_SIZE);

	if (len >= MAX_AUDIO_NAME_LENGTH)
	{
		base_name[MAX_AUDIO_NAME_LENGTH - 4] =
			base_name[MAX_AUDIO_NAME_LENGTH - 3] =
			base_name[MAX_AUDIO_NAME_LENGTH - 2] = '.';

		base_name[MAX_AUDIO_NAME_LENGTH - 1] = '\0';
	}

	strncpy(dest, base_name, MAX_AUDIO_NAME_LENGTH);
}


int alsa_install_pcm(snd_pcm_t **alsa_handle_ptr,
	unsigned int *rrate_ptr, snd_pcm_uframes_t *rframes_ptr,
	snd_pcm_stream_t stream_direction,
	int mode
	)
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
	err = snd_pcm_open(&alsa_handle, "default", stream_direction, mode);
	if (err < 0)
	{
		if (verbose())	printf("Failed to open ALSA PCM default device.\r\n");
		return err;
	}

	// Allocate a hardware parameters object
	err = snd_pcm_hw_params_malloc(&hw_params);
	if (err < 0)
	{
		if (verbose())	printf("Failed to malloc ALSA params.\r\n");
		return err;
	}

	// Fill it with default values
	err = snd_pcm_hw_params_any(alsa_handle, hw_params);
	if (err < 0)
	{
		if (verbose())	printf("Failed to fill params with default values.\r\n");
		return err;
	}

	// From now on we set desired hardware parameters.

	// Interleaved mode (we'll use only one channel though, since it's mono)
	err = snd_pcm_hw_params_set_access(alsa_handle, hw_params,
									   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0)
	{
		if (verbose())	printf("Failed to set interleaved mode on ALSA PCM.\r\n");
		return err;
	}

	// Signed 16-bit little-endian format
	err = snd_pcm_hw_params_set_format(alsa_handle, hw_params,
									   SND_PCM_FORMAT_S16_LE);
	if (err < 0)
	{
		if (verbose())	printf("Failed to set 16bit LE format on ALSA PCM.\r\n");
		return err;
	}

	// Setting the sampling rate.
	// After this call rrate contains the real rate at which the device will
	// record
	err = snd_pcm_hw_params_set_rate_near(alsa_handle, hw_params, &rrate, 0);
	if (err < 0)
	{
		if (verbose())	printf("Failed to set sampling rate on ALSA PCM.\r\n");
		return err;
	}

	// Setting one channel only (mono)
	err = snd_pcm_hw_params_set_channels(alsa_handle, hw_params, 1);
	if (err < 0)
	{
		if (verbose())	printf("Failed to set mono channel on ALSA PCM.\r\n");
		return err;
	}

	// Setting execution period size based on number of frames of the buffer
	// After this call, rframes will contain the actual period accepted by the
	// device
	err = snd_pcm_hw_params_set_period_size_near(alsa_handle, hw_params,
												 &rframes, 0);
	if (err < 0)
	{
		if (verbose())	printf("Failed to set period on ALSA PCM.\r\n");
		return err;
	}

	// Writing parameters to the driver
	err = snd_pcm_hw_params(alsa_handle, hw_params);
	if (err < 0)
	{
		if (verbose())	printf("Failed to write params to ALSA PCM device.\r\n");
		return err;
	}

	// Freeing up local resources allocated via malloc
	// NOTICE: on failure the program aborts, so there is no need to free them
	// each time a call fails
	snd_pcm_hw_params_free(hw_params);

	// Copying into INOUT parameters
	*alsa_handle_ptr	= alsa_handle;
	*rrate_ptr			= rrate;
	*rframes_ptr		= rframes;

	return 0;
}

void copy_audio_with_padding(double *out_buffer, const short *in_buffer)
{
size_t i;
	// NOTICE: it's not a coincidence I'm using record.rframes instead
	// than fft.rframes, because the two differ in the case zero-padding
	// is enabled!
	for (i = 0; i < audio_state.record.rframes; ++i)
	{
		out_buffer[i] = (double)in_buffer[i];
	}

	// Reset zero padding if needed, hence if record.rframes < fft.rframes;
	// This loop starts with i equal to audio_state.record.rframes
	for (; i < audio_state.fft.rframes; ++i)
	{
		out_buffer[i] = 0.;
	}
}


// TODO: move somewhere else
void fft(double* data)
{
	fftw_execute_r2r(audio_state.fft.plan, data, data);
}

void ifft(double* data)
{
size_t i;

	fftw_execute_r2r(audio_state.fft.plan_inverse, data, data);

	// Normalization, because FFTW computes unnormalized FFT/IFFT
	for (i = 0; i < audio_state.fft.rframes; ++i)
	{
		data[i] /= audio_state.fft.rframes;
	}
}


/**
 * Initialize ALSA recorder handle.
 * TODO: document arguments
 */
int alsa_install_recorder(snd_pcm_t **record_handle_ptr,
	unsigned int *rrate_ptr, snd_pcm_uframes_t *rframes_ptr)
{
	return alsa_install_pcm(record_handle_ptr, rrate_ptr, rframes_ptr,
		SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
}

/**
 * Initialize ALSA playback handle.
 * TODO: document arguments
 */
int alsa_install_playback(snd_pcm_t **playback_handle_ptr,
	unsigned int *rrate_ptr, snd_pcm_uframes_t *rframes_ptr)
{
	// The zero indicates the blocking mode
	return alsa_install_pcm(playback_handle_ptr, rrate_ptr, rframes_ptr,
		SND_PCM_STREAM_PLAYBACK, 0);
}

/**
 * Initializes both Allegro sound and ALSA library to record
 */
static inline int allegro_alsa_sound_init(unsigned int *rrate_ptr,
	snd_pcm_uframes_t *rframes_ptr, snd_pcm_t **record_handle_ptr,
	snd_pcm_t **playback_handle_ptr)
{
int err;
int index;

void *cab_pointers[AUDIO_NUM_BUFFERS];
								// Pointers to buffers used in cab library

	// Allegro sound initialization
	// IDEA: MIDI files do not work, consider enabling MIDI_AUTODETECT and
	// implement MIDI files support
	err = install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL);
	if (err) return err;

	// Initialization of ALSA recorder
	err = alsa_install_recorder(record_handle_ptr, rrate_ptr, rframes_ptr);
	if (err) return err;

	// Initialization of ALSA playback
	err = alsa_install_playback(playback_handle_ptr, rrate_ptr, rframes_ptr);
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
		AUDIO_DESIRED_BUFFER_SIZE,
		cab_pointers);

	return err;
}

/**
 * Initializes FFTW3 library to perform FFT analysis
 */
static inline int fft_init(snd_pcm_uframes_t rframes,
	fftw_plan* fft_plan_ptr,
	fftw_plan* fft_plan_inverse_ptr)
{
int err;
int index;

void *cab_pointers[AUDIO_NUM_BUFFERS];
								// Pointers to buffers used in cab library

double *inout;					// Array that will be used to create a
								// FFTW3 plan

char wisdom_filepath[MAX_CHAR_BUFFER_SIZE];

	// Getting absolute path for wisdom file location
	strncpy(wisdom_filepath, working_directory(), MAX_CHAR_BUFFER_SIZE);
	strncat(wisdom_filepath, "super_wisdom.dat",
		MAX_CHAR_BUFFER_SIZE - strlen(wisdom_filepath));

	// Wisdom is a way to speed up plan generation for FFTW library.
	// Wisdom is cumulative, hence each time the rframes value changes for
	// whatever reason the new wisdom file generated will contain parameters
	// for both past and current value of rframes variable.
	err = fftw_import_wisdom_from_filename(wisdom_filepath);

	if (err == 0)
	{
		printf("Could not load FFT Wisdom from dat file, "
			"program initialization will surely take longer...\r\n");
	}

	// Converting to the number of frames comprensive of padding
	rframes = AUDIO_ADD_PADDING(rframes);

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

	// The returned plan is for the inverse FFT
	*fft_plan_inverse_ptr = fftw_plan_r2r_1d(rframes,
											 inout,
											 inout,
											 FFTW_HC2R,
											 FFTW_EXHAUSTIVE); // OR FFTW_PATIENT

	// Saving back updated wisdom to dat file
	fftw_export_wisdom_to_filename(wisdom_filepath);

	// From now on we can calculate the FFT using the given plan, by calling the
	// following function:
	// fftw_execute_r2r(const fftw_plan p, double *in, double *out);

	// Deallocation of the array
	fftw_free(inout);

	// Construction of CAB pointers for fft buffers
	for (index = 0; index < AUDIO_NUM_BUFFERS; ++index)
	{
		cab_pointers[index] = STATIC_CAST(void*, &audio_state.fft.buffers[index]);
	}

	// Initializing FFT CAB buffers, the pointers are copied to the cab
	// structure
	err = ptask_cab_init(&audio_state.fft.cab,
						 AUDIO_NUM_BUFFERS,
						 AUDIO_DESIRED_PADBUFFER_SIZE,
						 cab_pointers);

	return err;
}

/**
 * Initializes analysis data structure.
 */
static inline int analysis_init()
{
int err;
int index;

// FIXME: normalize use of AUDIO_MAX_FILES and AUDIO_NUM_BUFFERS constants
void *cab_pointers[AUDIO_MAX_FILES];	// Pointers to buffers used in cab library

	// Construction of CAB pointers for analysis buffers
	for (index = 0; index < AUDIO_MAX_FILES; ++index)
	{
		cab_pointers[index] = STATIC_CAST(void*, audio_state.analysis.buffers[index]);
	}

	// Initializing FFT CAB buffers, the pointers are copied to the cab
	// structure
	err = ptask_cab_init(&audio_state.analysis.cab,
						 AUDIO_NUM_BUFFERS,
						 AUDIO_DESIRED_PADBUFFER_SIZE,
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
	return snd_pcm_prepare(audio_state.record.record_handle);
}

static inline int playback_prepare()
{
	return snd_pcm_prepare(audio_state.record.playback_handle);
}

/**
 * Stops the microphone, dropping immediately any buffered frame.
 * It is used when it's sure that no further frame will be needed from now
 * until the next prepare.
 * TODO: check what are the return values.
 */
static inline int mic_stop()
{
	return snd_pcm_drop(audio_state.record.record_handle);
}

/**
 * Reads microphone data if available, microphone is non-blocking so this call
 * returns immediately with the number of read frames. Returns 0 if there were
 * no frame to read, less than zero on error.
*/
static inline int mic_read(short* buffer, const int nframes)
{
int err;

	err = snd_pcm_readi(audio_state.record.record_handle,
						STATIC_CAST(void *, buffer),
						nframes);

	// NOTICE: This assumes no recovery code is available to be executed from
	// erroneus states. See mic_read_blocking.
#ifdef NDEBUG
	if (err < 0 && err != -EAGAIN)
		abort_on_error("Bad ALSA read!");
#endif

	return err;
}

/**
 * Waits for a specified amount of ms.
 * If interrupted the wait is resumed with the remaining time, thus this
 * function always waits for the specified amount of ms.
 */
static inline void timed_wait(int ms)
{
int s = 0;

	if (ms < 1)
		return;

	if (ms >= 1000) {
		s = ms / 1000;
		ms = ms % 1000;
	}

	const struct timespec req = {
		.tv_sec = s,
		.tv_nsec = ms * 1000000L,
	};

	// The nanosleep updates req with the remaining time if interrupted, thus
	// even if interrupted the call waits always for the given amount of ms
	// before terminating.
	while(clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL))
		;
}

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

snd_pcm_t *record_handle;		// ALSA Hardware Handle used to record audio
snd_pcm_t *playback_handle;		// ALSA Hardware Handle used to playback recorded audio

fftw_plan fft_plan;				// The FFTW3 plan, which is the algorithm that will
								// be used to calculate the FFT, optimized for
								// the size of the recording buffer

fftw_plan fft_plan_inverse;		// The FFTW3 plan used to compute the inverse FFT

	// Mutex initialization
	err = ptask_mutex_init(&audio_state.mutex);
	if (err) return err;

	// Allegro and ALSA initialization
	err = allegro_alsa_sound_init(&rrate, &rframes, &record_handle, &playback_handle);
	if (err) return err;

	// FFTW3 initialization
	err = fft_init(rframes, &fft_plan, &fft_plan_inverse);
	if (err) return err;

	// Analysis data structures initialization
	err = analysis_init();
	if (err) return err;

	// Copy local vales to global structures
	audio_state.record.rrate			= rrate;
	audio_state.record.rframes			= rframes;
	audio_state.record.record_handle	= record_handle;
	audio_state.record.playback_handle	= playback_handle;

	audio_state.fft.rrate			= rrate;
	audio_state.fft.rframes			= AUDIO_ADD_PADDING(rframes);
	audio_state.fft.plan			= fft_plan;
	audio_state.fft.plan_inverse	= fft_plan_inverse;

	return 0;
}

int audio_open_file(const char *filename)
{
audio_pointer_t	file_pointer;	// Pointer to the opened file
audio_type_t	file_type;		// Detected file type
int				index;			// Index of newly used audio file descriptor

	if (audio_state.opened_audio_files >= AUDIO_MAX_FILES)
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
		audio_file_copy(&audio_state.audio_files[index], &audio_file_new);
		audio_state.audio_files[index].type = file_type;
		path_to_basename(audio_state.audio_files[index].filename, filename);
		audio_state.audio_files[index].datap = file_pointer;

		++audio_state.opened_audio_files;
	}

	// If the pointer is NULL then the file was an invalid input
	return file_pointer.gen_p ? 0 : EINVAL;
}

bool audio_is_file_open(int i)
{
	return i >= 0 && i < audio_state.opened_audio_files;
}

int audio_file_opened()
{
	return audio_state.opened_audio_files;
}

int audio_close_file(int filenum)
{
int err = 0;

	if (audio_is_file_open(filenum))
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

		--audio_state.opened_audio_files;

		// Shift back array, starting from position of filenum
		for(; filenum < audio_state.opened_audio_files; ++filenum)
		{
			audio_file_copy(
				&audio_state.audio_files[filenum],
				&audio_state.audio_files[filenum+1]
			);
		}

		// audio_state.audio_files[audio_state.opened_audio_files] = audio_file_new;
	}
	else
		err = EINVAL;

	return err;
}

bool audio_file_has_rec(int i)
{
	return audio_state.audio_files[i].has_rec;
}

char* audio_file_name(int i)
{
	return audio_state.audio_files[i].filename;
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

int audio_get_record_rrate() {
	return audio_state.record.rrate;
}

int audio_get_record_rframes() {
	return audio_state.record.rframes;
}

int audio_get_fft_rrate() {
	return audio_state.fft.rrate;
}

int audio_get_fft_rframes() {
	return audio_state.fft.rframes;
}

int audio_get_num_files()
{
	// Safe since the number of opened files cannot be modified in multithreaded
	// environment
	return audio_state.opened_audio_files;
}

const char* audio_get_filename(int i)
{
	if (!audio_is_file_open(i))
		return NULL;

	// Safe since the files cannot be opened/closed in multithreaded
	// environment
	return audio_state.audio_files[i].filename;
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

// -------------- BUFFERS FUNCTIONS --------------

int audio_get_last_record(const short* buffer_ptr[], int* buffer_index_ptr)
{
int err;

	err = ptask_cab_getmes(&audio_state.record.cab,
		STATIC_CAST(const void**, buffer_ptr),
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

int audio_get_last_fft(const double *buffer_ptr[], int *buffer_index_ptr)
{
int err;
fft_output_t *fft_pointer;	// Pointer to the fft structure obtained from the cab

	err = ptask_cab_getmes(&audio_state.fft.cab,
		STATIC_CAST(const void **, &fft_pointer),
		buffer_index_ptr,
		NULL);

	if (err)
		return -EAGAIN;

	// Since the fft is stored with autocorrelation, we extract it
	*buffer_ptr = fft_pointer->fft;

	return audio_state.fft.rframes;
}

void audio_free_last_fft(int buffer_index)
{
	ptask_cab_unget(&audio_state.fft.cab, buffer_index);
}

// -------------- MICROPHONE --------------

/**
 * Reads microphone data if available, blocking until the number of frames that
 * is requested is not available yet.
 * It returns zero on success, a non zero value on failure.
 * TODO: add more to this documentation if the function is actually used.
 * TODO: actually probably this function will be used while in single-thread text mode.
*/
static inline int mic_read_blocking(short* buffer, const int nframes)
{
int err;
int how_many_read = 0;	// How many frames have already been read
int missing = nframes;	// How many frames are still missing

	while (missing > 0)
	{
		// Non-blocking read
		err = mic_read(buffer + how_many_read, missing);

		if (err < 0)
		{

#ifdef NDEBUG
			// Release mode, function returns with an error if an error
			// different than -EAGAIN occurs
			switch (err)
			{
			case -EAGAIN:
				// No big deal, we can suspend for some time
				// See after the switch block
				break;
			case -EBADFD:
				if (verbose())
					printf("ALSA device was not in correct state.");
				return err;
			case -EPIPE:
				// NOTICE: Could implement some code to recover from this state
				if (verbose())
					printf("Overrun in ALSA microphone handling.");
				return err;
			case -ESTRPIPE:
				// NOTICE: Could implement some code to recover from this state
				if (verbose())
					printf("ALSA suspend event occurred.");
				return err;
			default:
				if (verbose())
					printf("ALSA unexpected error in blocking recording!");
				return err;
			}
#else
			// In debug mode, program prints some extra information, but aborts
			// if an error occurs
			switch(err)
			{
			case -EAGAIN:
				// No big deal, we can suspend for some time
				// See after the switch block
				break;
			case -EBADFD:
				printf("ALSA device was not in correct state: %s.\r\n",
						snd_strerror(err));
				assert(false);

				break;
			case -EPIPE:
				// NOTICE: Could implement some code to recover from this state
				printf("ALSA overrun: %s.\r\n", snd_strerror(err));
				assert(false);

				break;
			case -ESTRPIPE:
				// NOTICE: Could implement some code to recover from this state
				printf("WARN: ALSA suspend event: %s.\r\n", snd_strerror(err));
				assert(false);

				break;
			default:
				printf("ALSA unexpected error: %s.\r\n", snd_strerror(err));
				assert(false);

				break;
			}

			printf("LOG: missing=%d, delay=%ld (ms).\r\n", missing,
				FRAMES_TO_MS(missing, audio_state.record.rrate));
#endif

			// The delay is an estimation based on the number of missing frames
			timed_wait(FRAMES_TO_MS(missing, audio_state.record.rrate));
		}
		else
		{
			how_many_read	+= err;
			missing			-= err;
		}
	}

	return 0;
}

/**
 * Waits for the given amount of seconds, printing a countdown on the standard
 * output every second, followed by an exclamation mark when the countdown is
 * finished.
 * TODO: move
 */
static inline void wait_seconds_print(int nseconds)
{
	for (; nseconds > 0; --nseconds)
	{
		printf("%d . . . ", nseconds);
		fflush(stdout);
		timed_wait(1000);
	}
	printf("!\r\n");
}

int real_index(int i)
{
	return i;
}

int imaginary_index(int i)
{
	return audio_state.fft.rframes - i;
}

void cross_correlation(double *output, const double *first_fft, const double *second_fft)
{
int i;
int re_idx;
int im_idx;
int last_idx;

	// Number of complex numbers composing the fft
	int number_complex = AUDIO_FRAMES_TO_HALFCOMPLEX(audio_state.fft.rframes);

	// I need to multiply element-wise complex numbers from the first fft to the
	// complex conjugate of the second_fft, then calculate the inverse fft of
	// the output

	// First element is always pure real
	output[0] = first_fft[0] * second_fft[0];

	// Last element is also always pure real (if present)
	// NOTICE: this element is computed only if the rframes number is even
	if (IS_EVEN(audio_state.fft.rframes))
	{
		last_idx = audio_state.fft.rframes / 2;
		output[last_idx] = first_fft[last_idx] * second_fft[last_idx];
	}

	// Starting from second element, until AUDIO_FRAMES_TO_HALFCOMPLEX(fft.rframes)
	// multiply together complex numbers:
	//	out	= a * conj(b)
	//		= ...
	//		= re(a)*re(b) + im(a)*im(b) + j*(im(a)*re(b) - re(a)*im(b))

	// Thus we can calculate separately the two values as
	// re(out) = re(a)*re(b) + im(a)*im(b)
	// im(out) = im(a)*re(b) - re(a)*im(b)

	for(i = 1; i <= number_complex; ++i)
	{
		re_idx = real_index(i);
		im_idx = imaginary_index(i);
		output[re_idx] = first_fft[re_idx] * second_fft[re_idx]
					   + first_fft[im_idx] * second_fft[im_idx];
		output[im_idx] = first_fft[im_idx] * second_fft[re_idx]
					   - first_fft[re_idx] * second_fft[im_idx];
	}

	// Finally, compute the inverse fft
	ifft(output);
}

double max(double *v, size_t n)
{
size_t i;
double m = v[0];

	for (i = 1; i < n; ++i)
	{
		if (m < v[i])
			m = v[i];
	}

	return m;
}

double unnormalized_cross_correlation(const double *first_fft, const double *second_fft)
{
double *buffer;
ptask_cab_id_t index;

	// Cannot fail
	ptask_cab_reserve(&audio_state.analysis.cab,
		STATIC_CAST(void **, &buffer),
		&index);

	cross_correlation(buffer, first_fft, second_fft);

	double correlation_value = max(buffer, audio_state.fft.rframes);

	ptask_cab_unget(&audio_state.analysis.cab, index);

	return correlation_value;
}

double normalized_cross_correlation(
	const double *first_fft, const double *second_fft,
	double first_autocorr, double second_autocorr)
{
	double unnormalized = unnormalized_cross_correlation(first_fft, second_fft);

	return (unnormalized * unnormalized) / (first_autocorr * second_autocorr);
}

int record_sample(short* buffer)
{
int err;

	// Preparing the microphone interface to be used
	err = mic_prepare();
	if (err)
	{
		if (verbose())
			printf("Could not prepare the microphone for audio acquisition.\r\n");
		return err;
	}

	err = mic_read_blocking(buffer, audio_state.record.rframes);
	if (err)
	{
		if (verbose())
			printf("Could not record properly the trigger sample!\r\n");
		return err;
	}

	// Stop recording
	err = mic_stop();
	if (err)
	{
		if (verbose())
			printf("Could not properly stop the microphone acquisition!\r\n");
		return err;
	}

	return 0;
}

int record_sample_to_play(int i)
{
int err;

	if (!audio_is_file_open(i))
	{
		if (verbose())
			printf("The specified audio file index is invalid!\r\n");
		return EINVAL;
	}

	// Since we overwrite the previous one, we mark temprarily that this file
	// has no sample associated with it
	audio_state.audio_files[i].has_rec = false;

	// Wait a few seconds to let the user get the timing right
	wait_seconds_print(COUNTDOWN_SECONDS);

	// Record it
	err = record_sample(audio_state.audio_files[i].recorded_sample);
	if(err)
		return err;

	audio_state.audio_files[i].has_rec = true;

	// Calculate the FFT of the signal once for later use
	copy_audio_with_padding(audio_state.audio_files[i].recorded_fft,
							audio_state.audio_files[i].recorded_sample);

	fft(audio_state.audio_files[i].recorded_fft);

	// Calculate autocorrelation once for later use, defined as the
	// cross-correlation with itself
	audio_state.audio_files[i].autocorr = unnormalized_cross_correlation(
		audio_state.audio_files[i].recorded_fft,
		audio_state.audio_files[i].recorded_fft
	);

	return 0;
}

/**
 * Play the given buffer on the default alsa playback handle, waiting for its
 * completion too.
 * The handle needs to be prepared first.
 * Returns zero on success, a non-zero value otherwise.
 */
static inline int playback_buffer_blocking(short* buffer, int size)
{
int remaining = size;
int err;

	while (remaining > 0)
	{
		err = snd_pcm_writei(audio_state.record.playback_handle,
			buffer,
			remaining);

		if (err < 0)
			abort_on_error("Could not play the recorded sample!");

		remaining -= err;
	}

	return 0;
}

/**
 * Mark that no more data will be streamed and close the hardware handle
 */
static inline int playback_stop()
{
	return snd_pcm_drain(audio_state.record.playback_handle);
}

void play_recorded_sample(int i)
{
int err;

	if (!audio_is_file_open(i) || !audio_state.audio_files[i].has_rec)
	{
		printf("The specified file does not exist or has no associated recording!\r\n");
		return;
	}

	err = playback_prepare();
	if (err)
		abort_on_error("ALSA PLAYBACK FAILURE!");

	err = playback_buffer_blocking(
		audio_state.audio_files[i].recorded_sample, audio_state.record.rframes);
	if (err)
		abort_on_error("ALSA PLAYBACK FAILURE!");

	err = playback_stop();
	if (err)
		abort_on_error("ALSA PLAYBACK FAILURE!");
}

void discard_recorded_sample(int i)
{
	audio_state.audio_files[i].has_rec = false;
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
		// NOTE: Change with a maximum time limit if this is a problem.
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
	// Stop recording
	err = mic_stop();
	if (err)
		abort_on_error("Could not stop properly the microphone acquisition.");


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
/// NOTICE: this task code is not reentrant, you can't have mutiple FFT Tasks
/// running concurrently at the same moment
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
const short *in_buffer;	// Pointer to local input buffer

int out_buffer_index;	// Index of local buffer used to produce fft data
double *out_buffer;		// Pointer to local output buffer

static double cross_corr_buffer[AUDIO_DESIRED_PADBUFFER_SIZE];
						// Buffer used to calculate cross-correlation

fft_output_t *out_pointer;

	tp = STATIC_CAST(ptask_t *, arg);
	// task_id = ptask_get_id(tp);

	// Variables initialization and initial computation

	ptask_start_period(tp);

	while (!main_get_tasks_terminate())
	{
		err = ptask_cab_getmes(&audio_state.record.cab,
				STATIC_CAST(const void **, &in_buffer),
				&in_buffer_index,
				&new_timestamp);

		// If the acquired buffer has not been analyzed yet
		if (err != EAGAIN && time_cmp(last_timestamp, new_timestamp) < 0)
		{
			/*
			struct timespec prova;
			time_diff(&prova, new_timestamp, last_timestamp);

			printf("TASL_FFT since last SAMPLE: %lld.%.9ld seconds.\r\n", (long long)prova.tv_sec, prova.tv_nsec);
			*/

			last_timestamp = new_timestamp;

			// Get buffer on which operate
			ptask_cab_reserve(&audio_state.fft.cab,
				STATIC_CAST(void **, &out_pointer),
				&out_buffer_index);

			out_buffer = out_pointer->fft;

			// Copy data into new buffer
			copy_audio_with_padding(out_buffer, in_buffer);

			// NOTICE: To be quickier I should release the in_buffer as soon as
			// I get here, but this could lead to some confusion because there
			// would be two lines that may release the buffer, depending on a
			// condition, I prefer releasing it after the analysis

			// Calculate in-place FFT using the plan computed above on
			// current buffer (potentially zero-padded)
			fft(out_buffer);

			// TODO: Should calculate its autocorrelation and publish that too somewhere, along with the fft
			out_pointer->autocorr = unnormalized_cross_correlation(
				out_buffer,
				out_buffer
			);

			// NOTICE: Output of previous FFT is an halfcomplex notation of the
			// FFT, it should be changed to a full array of magniutes to be
			// printed.

			// Publish new FFT
			ptask_cab_putmes(&audio_state.fft.cab, out_buffer_index);
		}
		// Otherwise, do nothing

		// Realease acquired buffer (if acquired)
		if (err == 0)
			ptask_cab_unget(&audio_state.record.cab, in_buffer_index);

		if (ptask_deadline_miss(tp))
			printf("TASK_FFT missed %d deadlines!\r\n", ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup

	// The reset can be done here because the only task that reserves buffers
	// for writing purposes is this one
	ptask_cab_reset(&audio_state.fft.cab);

	return NULL;
}

/// The body of the analyzer task
void *als_task(void *arg)
{
ptask_t *tp; // task pointer
// int task_id; // task identifier

// Local variables
// int err;

struct timespec last_timestamp = { .tv_sec = 0, .tv_nsec = 0};
						// Timestamp of last accessed input buffer
struct timespec new_timestamp;
						// Timestamp of the new input buffer

// int in_buffer_index;	// Index of local buffer used to get microphone data
// const double *in_buffer;	// Pointer to local input buffer

// TODO: there is no out buffer, but there may be a local copy

// unsigned int i;			// Index used to copy data
int file_index;			// Index of the file that has been associated with this task

	tp = STATIC_CAST(ptask_t *, arg);
	// task_id = ptask_get_id(tp);

	// Get the associated file index as it is the only argument
	file_index = *STATIC_CAST(int*, &tp->args);

	// TODO: log function
	// printf("Started an analyzer task for the file of index %d!\r\n", file_index);

	// Variables initialization and initial computation

	ptask_start_period(tp);

const fft_output_t *fft_ptr;
ptask_cab_id_t buffer_id;
int err;
double correlation;

	while (!main_get_tasks_terminate())
	{
		// TODO: DO STUFF
		// TODO: check timestamp too
		err = ptask_cab_getmes(&audio_state.fft.cab,
			STATIC_CAST(const void **, &fft_ptr),
			&buffer_id,
			&new_timestamp
		);

		// If the acquired buffer has not been analyzed yet
		if (err != EAGAIN && time_cmp(last_timestamp, new_timestamp) < 0)
		{
			/*
			struct timespec prova;
			time_diff(&prova, new_timestamp, last_timestamp);

			printf("TASL_ALS since last FFT: %lld.%.9ld seconds.\r\n", (long long)prova.tv_sec, prova.tv_nsec);
			*/

			last_timestamp = new_timestamp;

			correlation = normalized_cross_correlation(
				audio_state.audio_files[file_index].recorded_fft,
				fft_ptr->fft,
				audio_state.audio_files[file_index].autocorr,
				fft_ptr->autocorr
			);

			// printf("Correlation with file %d is %f .\r\n", file_index+1, correlation);

			// TODO: magic number
			if (fabs(correlation) > 0.35) {
				// TODO: insert time decay
				audio_play_file(file_index);
			}
		}

		// Realease acquired buffer (if acquired)
		if (err == 0)
			ptask_cab_unget(&audio_state.fft.cab, buffer_id);

		if (ptask_deadline_miss(tp))
			printf("TASK_ALS for file %d missed %d deadlines!\r\n",
				file_index+1, ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup

	return NULL;
}
