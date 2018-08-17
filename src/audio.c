#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <libgen.h>			// Used for basename

#include <assert.h>			// Used in debug

#include <allegro.h>
#include <alsa/asoundlib.h>

#include "api/std_emu.h"	// Boolean type declaration
#include "api/ptask.h"

#include "constants.h"

#include "audio.h"
#include "main.h"

/* -----------------------------------------------------------------------------
 * CONSTANTS
 * -----------------------------------------------------------------------------
 */

#define MAX_VOL		255		// Maximum volume
#define MIN_VOL		0		// Minimum volume (silent)

#define CRX_PAN		255		// Right channel panning
#define CLX_PAN		0		// Left channel panning
#define MID_PAN		128		// Both channels panning

#define SAME_FRQ	1000	// Play the sample at the original
							// frequency

#define MAX_FREQ	9990	// Maximum frequency
#define MIN_FREQ	0		// Minimum frequency

#define MAX_AUDIO_FILES 8	// The maximum number of audio files
							// that can be opened at startup

#define MAX_AUDIO_NAME_LENGTH 32
							// The maximum length of the audio file name


/* -----------------------------------------------------------------------------
 * DATA TYPES
 * -----------------------------------------------------------------------------
 */

// Pointer to the opened file type
typedef union __AUDIO_POINTER_UNION
{
	SAMPLE* audio_p;		// Pointer to the audio SAMPLE structure
	MIDI*	midi_p;			// Pointer to the MIDI structure
	void*	gen_p;			// Fallback generic pointer
} audio_pointer_t;


// Opened audio file descriptor
typedef struct __AUDIO_FILE_DESC_STRUCT
{
	audio_pointer_t datap;	// Pointer to the opened file
	audio_type_t	type;	// File type
	int				volume;	// Volume used when playing this file
	int				panning;// Panning used when playing this file
	int				frequency;
							// Frequency used when playing this file,
							// 1000 is the base frequency

	// TODO:
	// bool loop;			// Tells if the audio should be reproduced in a loop

	char filename[MAX_AUDIO_NAME_LENGTH];
							// Name of the file displayed on the screen,
							// contains only the basename, ellipsed if too long

	bool			has_rec;// Tells if the file has an associated recorded
							// audio that can be recognized to start it playing

	// TODO: pointer of the SAMPLE of recognized sound
} audio_file_desc_t;

// TODO: change this struct to comply with ALSA code requirements, not Allegro,
// since Allegro doesn't work
/*
typedef struct __AUDIO_RECORD_INFO_STRUCT
{
	// TODO: remove this struct
} audio_record_info_t;
*/

typedef struct __AUDIO_RECORD_STRUCT
{
	unsigned int rrate;		// Recording acquisition ratio, as accepted by the
							// device
	snd_pcm_uframes_t rframes;
							// Period requested to recorder task, expressed in
							// terms of number of frames
	snd_pcm_t *alsa_handle; // ALSA Hardware Handle used to record audio

	short buffers[AUDIO_NUM_BUFFERS][AUDIO_DESIRED_FRAMES];
							// Buffers used within the cab
	ptask_cab_t cab;		// CAB used to handle allocated buffers
} audio_record_t;

// Global state of the module
typedef struct __AUDIO_STRUCT
{
	audio_file_desc_t audio_files[MAX_AUDIO_FILES];
							// Array of opened audio files for reproduction
	int opened_audio_files;	// Number of currently opened audio files

	audio_record_t record;	// Contains all the data needed to record

	ptask_mutex_t mutex;	// TODO: mutex to access this data structure
} audio_state_t;


// Base empty audio file descriptor
const audio_file_desc_t audio_file_new =
{
	.type		= AUDIO_TYPE_SAMPLE,
	.volume		= MAX_VOL,
	.panning	= MID_PAN,
	.frequency	= SAME_FRQ,
	.filename	= "",
	.has_rec	= false,
	/*
	.loop		= false,
	TODO:
	*/
};

/* -----------------------------------------------------------------------------
 * GLOBAL VARIABLES
 * -----------------------------------------------------------------------------
 */

audio_state_t audio_state =
{
	.opened_audio_files = 0,
	// TODO:
};

/* -----------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Copies the basename of the given path into dest, ellipsing it if too long.
 */
void _path_to_basename(char* dest, const char* path)
{
char	buffer[MAX_CHAR_BUFFER_SIZE];// Buffer, used to prevent path modifications
char	*base_name;
int		len;

	strncpy(buffer, path, MAX_CHAR_BUFFER_SIZE);

	base_name = basename(buffer);

	len = strnlen(buffer, MAX_CHAR_BUFFER_SIZE);

	if (len >= MAX_AUDIO_NAME_LENGTH)
	{
		base_name[MAX_AUDIO_NAME_LENGTH - 4] =
			base_name[MAX_AUDIO_NAME_LENGTH - 3] =
			base_name[MAX_AUDIO_NAME_LENGTH - 2] = '.';

		base_name[MAX_AUDIO_NAME_LENGTH - 1] = '\0';
	}

	strncpy(dest, base_name, MAX_AUDIO_NAME_LENGTH);
}



/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/* ------- UNSAFE FUNCTIONS - CALL ONLY IN SINGLE THREAD ENVIRONMENT -------- */

#define AUDIO_INPUT_MONO	0
#define AUDIO_INPUT_STEREO	1

/*
 * Initializes the mutex and the other required data structures.
 * TODO: split this body to multuple functions
 */
int audio_init()
{
int err;
int index;
void *cab_pointers[AUDIO_NUM_BUFFERS];
								// Pointers to buffers used in cab library
unsigned int rrate = AUDIO_DESIRED_RATE;
								// Recording acquisition ratio, as accepted by
								// the device
snd_pcm_uframes_t rframes = AUDIO_DESIRED_FRAMES;
								// Period requested to recorder task, expressed
								// in terms of number of frames
snd_pcm_t *alsa_handle;			// ALSA Hardware Handle used to record audio
snd_pcm_hw_params_t *hw_params; // Parameters used to configure ALSA hardware

	// Mutex initialization
	err = ptask_mutex_init(&audio_state.mutex);
	if (err) return err;

	// Allegro sound initialization
	err = install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL); // TODO: MIDI_AUTODETECT
	if (err) return err;

	// From now on, ALSA PCM code follows

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

	// TODO: this can be made a local buffer
	for (index = 0; index < AUDIO_NUM_BUFFERS; ++index)
	{
		cab_pointers[index] = STATIC_CAST(void*, audio_state.record.buffers[index]);
	}

	// Initializing the capture CAB buffers
	err = ptask_cab_init(&audio_state.record.cab,
		AUDIO_NUM_BUFFERS,
		AUDIO_DESIRED_FRAMES,
		cab_pointers);

	if (err) return err;

	// Copy local vales to global structure
	audio_state.record.rrate = rrate;
	audio_state.record.rframes = rframes;
	audio_state.record.alsa_handle = alsa_handle;

	return 0;
}

/*
 * Opens the file specified by the filename
 */
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

		_path_to_basename(audio_state.audio_files[index].filename, filename);

		audio_state.audio_files[index].datap = file_pointer;

		++audio_state.opened_audio_files;
	}

	// If the pointer is NULL then the file was an invalid input
	return file_pointer.gen_p ? 0 : EINVAL;
}

/*
 * Closes an opened audio file and shifts all the indexes of opened audio files
 * back if needed.
 */
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

		// TODO: if the file had a recorded trigger delete that too

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

/*
 * List audio files on stdandard output.
 */
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

/*
 * Plays the file specified by the number
 */
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

/*
 * Stops any audio or midi that is currently playing.
 */
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

/*
 * Getters functions
 */
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

/*
 * Setters functions
 */
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

/*
 * Modifiers functions
 */
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

/* -----------------------------------------------------------------------------
 * TASKS
 * -----------------------------------------------------------------------------
 */

// TODO: reduce this body
void* microphone_task(void *arg)
{
ptask_t*	tp;			// task pointer
int			task_id;	// task identifier

// Local variables
int		err;

// TODO: do not use local buffer, request a CAB first
int 	buffer_index;	// index of local buffer used to get microphone data
						// used to access cab
short	*buffer;		// pointer to local buffer, changes each execution

int		how_many_read;	// how many frames have been read from the stream
int		missing;		// how many are missing before the next acquisition

	tp = STATIC_CAST(ptask_t *, arg);
	task_id = ptask_get_id(tp);

	// Variables initialization and initial computation

	// Preparing the microphone interface to be used
	err = snd_pcm_prepare(audio_state.record.alsa_handle);
	if (err)
		abort_on_error("Could not prepare microphone acquisition.");

	ptask_start_period(tp);

	while (!main_get_tasks_terminate())
	{
		how_many_read	= 0;
		missing			= audio_state.record.rframes;

		// Get a local buffer from the CAB TODO: check wether this is fine
		// It never fails if the number of tasks is strictly less than the
		// number of available CABs, otherwise the behavior is unspecified
		ptask_cab_reserve(&audio_state.record.cab,
			STATIC_CAST(void*, &buffer),
			&buffer_index);

		// TODO: Move capture to another function

		// Capture audio from stream and write it to assigned CAB
		while (missing > 0)
		{
			err = snd_pcm_readi(audio_state.record.alsa_handle,
				STATIC_CAST(void *, buffer + how_many_read),
				missing);

			// TODO: once we have timed waits, decide how to handle better
			// undersampling

			if (err < 0)
			{
				switch (err)
				{
				case -EBADFD:
					fprintf(stderr,
							"ALSA device was not in correct state: %s.\r\n",
							snd_strerror(err));
					assert(false);
					break;
				case -EPIPE:
					// TODO: Overrun
					fprintf(stderr, "WARN: ALSA overrun: %s.\r\n", snd_strerror(err));
					break;
				case -ESTRPIPE:
					// TODO: Suspend event occurred
					fprintf(stderr,
							"WARN: ALSA suspend event occurred: %s.\r\n",
							snd_strerror(err));
					break;
				case -EAGAIN:
					fprintf(stderr, "WARN: ALSA EAGAIN.\r\n");
					break;
				default:
					fprintf(stderr,
							"WARN: ALSA unexpected error: %s.\r\n",
							snd_strerror(err));
					assert(false);
					break;
				}
			}
			else
			{
				fprintf(stderr, "LOG: read %d values.\r\n", err);
				how_many_read	+= err;
				missing			-= err;
			}
		}

		// Release CAB to apply changes
		ptask_cab_putmes(&audio_state.record.cab, buffer_index);

		// TODO: how do I know from within another thread wether this is new
		// data or data already read from that very thread?

		if (ptask_deadline_miss(tp))
			printf("TASK_MIC missed %d deadlines!\r\n", ptask_get_dmiss(tp));

		ptask_wait_for_period(tp);
	}

	// Cleanup
	ptask_cab_reset(&audio_state.record.cab);

	return NULL;
}
