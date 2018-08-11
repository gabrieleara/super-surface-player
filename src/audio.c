#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>			// Used for basename

#include <assert.h>			// Used in debug

#include <allegro.h>

#include "api/std_emu.h"	// Boolean type declaration
#include "api/ptask.h"

#include "constants.h"

#include "audio.h"

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

typedef struct __AUDIO_RECORD_INFO_STRUCT
{
	int cap_bits;		// Tells which kind of bit encoding the system supports
	int cap_stereo;		// Tells if the system is capable of recording stereo
						// input
	int max_frequency;	// Tells the maximum possible sample frequency
} audio_record_info_t;

typedef struct __AUDIO_RECORD_STRUCT
{
	audio_record_info_t info;
							// Contains all the info about the recording module
							// support
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
char	buffer[MAX_BUFFER_SIZE];// Buffer, used to prevent path modifications
char	*base_name;
int		len;

	strncpy(buffer, path, MAX_BUFFER_SIZE);

	base_name = basename(buffer);

	len = strnlen(buffer, MAX_BUFFER_SIZE);

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
 * Initializes the mutex TODO: and the other required data structures.
 */
int audio_init()
{
int err;
int cap_bits;		// Tells which kind of bit encoding the system supports
int cap_stereo;		// Tells if the system is capable of recording stereo input
int max_frequency;	// Tells the maximum possible sample frequency

	err = ptask_mutex_init(&audio_state.mutex);
	if (err) return err;

	// Allegro sound initialization
	err = install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL); // TODO: MIDI
	if (err) return err;

	// FIXME: SOUND INPUT DOES NOT WORK!

	// TODO: GABRI GUARDA QUA: rimuovi l'inizio del commento alla prossima riga
	// Per riabilitare l'input del microfono
/*
	err = install_sound_input(DIGI_AUTODETECT, MIDI_NONE);
	if (err) return err;
/*
	cap_bits = get_sound_input_cap_bits();
	if (cap_bits == 0)
	{
		// No audio input is supported, should not be possibile, since
		// install_sound_input returned a zero value.
		assert(false);
		return EINVAL;

	}
	else if (cap_bits & 16)
	{
		// We can have sixteen bit audio input. Good.
		// TODO:

		max_frequency = get_sound_input_cap_rate(16, AUDIO_INPUT_MONO);
	}
	else if (cap_bits & 8)
	{
		max_frequency = get_sound_input_cap_rate(8, AUDIO_INPUT_MONO);

		// We can have only eight bit audio input. Not so good.
		return EINVAL; // TODO: currently not supported
	}

	cap_stereo = get_sound_input_cap_stereo();

	audio_state.record.info.cap_bits = cap_bits;
	audio_state.record.info.cap_stereo = cap_stereo;
	audio_state.record.info.max_frequency = max_frequency;

	printf("\r\nAudio module settings:\r\n");
	printf("- cap_bits = %d\r\n", cap_bits);
	printf("- cap_stereo = %d\r\n", cap_stereo);
	printf("- max_frequency = %d\r\n\r\n", max_frequency);

	// TODO: put the following in another function, activated by a command in text mode:
	// get_sound_input_cap_parm(int rate, int bits, int stereo);

	*/
	return err;
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
