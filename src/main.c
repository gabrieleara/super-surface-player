/**
 * @file main.c
 * @brief Main program public functions and data types
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * The main module is reponsible for the setup of the application (in particular
 * when executed in text mode) and to manage tasks once the graphical mode is
 * started.
 *
 * For public functions, documentation can be found in corresponding header file:
 * video.h.
 *
 */

// Standard libraries
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <assert.h>			// Used in debug

// Linux-related types
#include <sys/types.h>

// POSIX directory management functions
#include <dirent.h>

// Linked libraries
#include <allegro.h>

// Custom libraries
#include "api/std_emu.h"
#include "api/ptask.h"

// Other modules
#include "constants.h"
#include "main.h"
#include "audio.h"
#include "video.h"

// -----------------------------------------------------------------------------
//                           PRIVATE DATA TYPES
// -----------------------------------------------------------------------------

/// Structure containing the global state of the program
typedef struct __MAIN_STRUCT
{
	// NOTE: When modifying the main_state_t data structure, consider
	// which state should be adopted as default state.

	bool			tasks_terminate;///< Tells if concurrent tasks should stop
									///< their execution
	bool			quit;			///< Tells if the program is shutting down
	bool			verbose;		///< Tells if the verbose flag has been set
	bool			wcet_analysis;	///< Tells if the wcet analysis flag as been set
	char			directory[MAX_DIRECTORY_LENGTH];
									///< The specified directory where to search
									///< for audio files (also referred as the
									///< current working directory)

	ptask_t			tasks[TASK_NUM];///< All the tasks data

	ptask_mutex_t	mutex;			///< Protects access to this data structure
	ptask_cond_t	cond;			///< used to wake up the main thread when in
									///< graphical mode
} main_state_t;

// -----------------------------------------------------------------------------
//                           GLOBAL VARIABLES
// -----------------------------------------------------------------------------

/// The state of the program
static main_state_t main_state =
{
	.tasks_terminate	= false,
	.quit				= false,
#ifdef NDEBUG
	.verbose			= false,
#else
	// In debug it is automatically verbose
	.verbose			= true,
#endif
	.wcet_analysis		= false,
	.directory			= "",
};


// -----------------------------------------------------------------------------
//                           PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------

bool verbose()
{
	return main_state.verbose;
}

bool wcet_analysis()
{
	return main_state.wcet_analysis;
}

char* working_directory()
{
	return main_state.directory;
}

void abort_on_error(char* message)
{
int len;

	if (message)
		printf("%s\r\n", message);

	len = strnlen(allegro_error, ALLEGRO_ERROR_SIZE);

	if (len > 0)
		printf("Last allegro error is %s.\r\n", allegro_error);

	// NOTE: A negative error may be due to ALSA library, consider using
	// snd_strerr to print the error.

	exit(EXIT_FAILURE);
}

void main_terminate_tasks()
{
	ptask_mutex_lock(&main_state.mutex);
	main_state.tasks_terminate = true;
	ptask_cond_signal(&main_state.cond);
	ptask_mutex_unlock(&main_state.mutex);
}

bool main_get_tasks_terminate()
{
bool res;

	ptask_mutex_lock(&main_state.mutex);
	res = main_state.tasks_terminate;
	ptask_mutex_unlock(&main_state.mutex);

	return res;
}


// -----------------------------------------------------------------------------
//                           PRIVATE FUNCTIONS
// -----------------------------------------------------------------------------

/* ----------------------- COMMAND-LINE ARGUMENTS --------------------------- */

/**
 * @name Command line arguments-related private functions
 */
//@{

/**
 * Checks an argument code. If the code is valid and it is the first time it is
 * specified enables that argument and returns zero, otherwise an error code is
 * returned.
 */
static inline int check_argument_code(char c)
{
int err = 0;

	switch (c)
	{
	// NOTE: Even if it is implemented, the verbose command does actually nothing
	// NOTE: Even if it is implemented, the wcet_analysis command does actually nothing
	case 'v':
#ifdef NDEBUG
		if(main_state.verbose)
			err = EINVAL;
		else
#endif
			// In debug mode it is automatically verbose, and errors due to
			// repeated flags are not checked
			main_state.verbose = true;
		break;

	case 'w':
		if (main_state.wcet_analysis)
			err = EINVAL;
		else
			main_state.wcet_analysis = true;
		break;

	// NOTE: More command line arguments may be placed.

	// TODO: Implement a command line that automatically updates WCET for each
	// task during program execution and prints at the end of each concurrent
	// execution the calculated values for each task.

	default:
		// Unknown argument
		err = EINVAL;
	}

	return err;
}

/**
 * Returns true if the given path exists and it is a directory.
 */
static inline bool is_valid_directory(const char* path)
{
DIR*	directory;		// Directory pointer, used to check if the directory
						// exists

	directory = opendir(path);

	if (directory != NULL)
	{
		closedir(directory);
		return true;
	}

	return false;
}

/**
 * Checks the command line arguments specified by the user.
 * Returns zero on success, an error code otherwise.
 */
static inline int read_arguments(int argc, char* argv[])
{
int			err = 0, i, len;
const char*	str;

int			cwdlen;
char 		cwd[MAX_CHAR_BUFFER_SIZE];	// It will contain the current directory
										// from which the program has been started.
bool		directory_already_specified = false;
										// If false we won't accept any other
										// directory specification in the
										// command line arguments

int			begin = 0;					// The index from which we will copy the
										// given directory in the main_state.directory

	// Get the directory in which the program has been launched
	getcwd(cwd, sizeof(cwd));
	cwdlen = strlen(cwd);

	// NOTICE: I know that for very very long paths it may get outside the array,
	// it is just very unlikely that a path is over 255 characters long.
	// On FAT or EXT4 systems it is even impossible, thus we will assume it is
	// always impossible for the sake of simplicity.

	// Last character must be a slash
	if (cwd[cwdlen-1] != '/') {
		cwd[cwdlen] = '/';
		cwd[cwdlen+1] = '\0';
		++cwdlen;
	}

	// Copy the cwd in the current directory, as a base for the optional extra
	// path given as command argument
	strcpy(main_state.directory, cwd);

	// The path optionally given as argument will be appended to the cwd if it
	// is not an absolute path.
	begin = cwdlen;

	// Iterate all command line arguments
	for (i = 1; i < argc && !err; ++i)
	{
		// Get current argument
		str = argv[i];

		if(str[0] == '-')
		{
			// It shall be a command line code specifier (minus sign + a character)
			if(strlen(str) != 2)
				err = EINVAL;
			else
				err = check_argument_code(str[1]);
		}
		else
		{
			// It shall be a directory specification
			len = strlen(str);

			if (directory_already_specified
				|| len >= MAX_DIRECTORY_LENGTH-1
				|| !is_valid_directory(str))
			{
				// Directory was already specified or is invalid
				err = EINVAL;
			}
			else
			{
				if (str[0] == '/') {
					begin = 0;
				}

				strncpy(main_state.directory + begin, str, MAX_DIRECTORY_LENGTH);
				len += begin;

				// Last character must be a slash
				if(main_state.directory[len-1] != '/')
				{
					main_state.directory[len] = '/';
					main_state.directory[len+1] = '\0';
				}
			}
		}
	}

	return err;
}

//@}

/* ------------------------- TERMINAL MODE COMMANDS ------------------------- */

/**
 * @name Terminal mode commands implementations
 */
//@{

/**
 * Displays all available commands in terminal mode.
 */
static inline void cmd_help()
{
	printf("\r\n");
	printf("Available commands and their effects:\r\n\r\n");

	printf(" close\t<fnum>\tTo close an already opened audio/midi file.\r\n");
	printf(" help\t\tTo show this help.\r\n");
	printf(" list\t\tList all the opened audio/midi files.\r\n");
	printf(" listen\t<fnum>\tListen to the specified audio/midi file.\r\n");
	printf(" play\t\tTo start playing in windowed mode.\r\n");
	printf(" pwd\t\tPrint current working directory.\r\n");
	printf(" open\t<fname>\tTo open a new audio/midi file.\r\n");
	printf(" quit\t\tTo quit this program.\r\n");
	printf(" record\t<fnum>\tTo record an audio input that will trigger the file specified by the num.\r\n");

	printf("\r\n");

	printf(" \t\tTo see the <fnum> associated to a file, use the list command.\r\n");

	printf("\r\n");

	printf(" \t\tThe specified <fname> shall be an absolute path or a relative "
			"path to the\r\n\t\tcurrent working directory.\r\n");

	printf("\r\n");
}

/**
 * Prints the current working directory.
 */
static inline void cmd_pwd()
{
	printf("Current working dir: %s\r\n", main_state.directory);
}

/**
 * Opens an audio input file, being it a sample or a midi it doesn't matter.
 */
static inline void cmd_open(char* filename)
{
char	buffer[MAX_CHAR_BUFFER_SIZE];
int		err = 0;

	if(filename[0] != '/')
	{
		strcpy(buffer, main_state.directory);
		strncat(buffer, filename, sizeof(buffer));
	}
	else
	{
		// Absolute path
		strncpy(buffer, filename, sizeof(buffer));
	}

	err = audio_open_file(buffer);

	if (err)
	{
		switch (err)
		{
		case EINVAL:
			printf("Could not load the specified file.\r\n");
			break;
		case EAGAIN:
			printf("Cannot open any more files, close at least one file to open the specified one.\r\n");
			break;
		default:
			assert(false);
		}
	}
	else
	{
		printf("The requested file has been opened.\r\n");
	}
}

/**
 * Plays an already-opened audio file, given its index in a 1-based list.
 */
static inline void cmd_listen(int fnum)
{
int err;

	// File numbers are specified as listed by the audio_list_fles,
	// but are played using zero based indexes.
	err = audio_play_file(fnum-1);

	if (err)
	{
		printf("The specified file could not be played.\r\n");
	}
}

/**
 * Closes an already-opened audio file, given its index in a 1-based list.
 */
static inline void cmd_close(int fnum)
{
int err;

	// File numbers are specified as listed by the audio_list_fles,
	// but are played using zero based indexes.
	err = audio_close_file(fnum-1);

	if (err)
	{
		printf("The specified file could not be closed.\r\n");
	}
}

static inline void wait_enter()
{
char buffer[MAX_CHAR_BUFFER_SIZE];
	printf("Press ENTER to continue...");
	fgets(buffer, sizeof(buffer), stdin);
}

static inline bool ask_yes_no(char* query)
{
char buffer[MAX_CHAR_BUFFER_SIZE];	// Buffer containing the inserted values
char *token;
bool answer = false;
bool yes = false;

	while(!answer) {
		printf("%s [y/n] ", query);

		fgets(buffer, sizeof(buffer), stdin);
		token = strtok(buffer, "\r\n");

		if (token == NULL)
		{
			// Do nothing, reask
		} else if (strcmp(token, "y") == 0 || strcmp(token, "yes") == 0) {
			answer = true;
			yes = true;
		} else if (strcmp(token, "n") == 0 || strcmp(token, "no") == 0) {
			answer = true;
			yes = false;
		} else {
			printf("Please, answer either \"yes\" or \"no\".\r\n");
		}
	}

	return yes;
}

static inline void cmd_list_audio_files()
{
int i;

	if (!audio_get_num_files())
	{
		printf("No audio files are opened.\r\n");
	}
	else
	{
		for(i = 0; i < audio_get_num_files(); ++i)
		{
			printf("\t%d. %s", i+1, audio_file_name(i));

			if (audio_file_has_rec(i))
				printf(" *");

			printf("\r\n");
		}
		printf("\r\nFiles with a * have an associated recorded sample.\r\n\r\n");
	}
}


static inline void cmd_record(int fnum)
{
bool yes		= false;
bool accepted	= false;
bool again		= true;
int err;

	if (!audio_is_file_open(fnum-1))
	{
		printf("A wrong file number has been specified. Aborted.\r\n");
		return;
	}

	printf("\r\nThe program will now prepare to record a new sample for "
		"file number %d.\r\n", fnum);

	yes = ask_yes_no("This will override any previously recorded sample "
		"associated with said audio file, are you sure?");

	if (!yes)
	{
		printf("Aborted!\r\n");
		return;
	}

	while(!accepted && again) {
		printf("You choose to record a new entry, the program will start recording after exactly 5 seconds after your next input.\r\n");

		wait_enter();
		printf("\r\n");

		err = record_sample_to_play(fnum-1);

		if (err)
		{
			printf("Error occurred while recording!\r\n");
			return;
		}

		printf("Recorded!\r\nThe recorded sample will now be played.\r\n");

		wait_enter();
		printf("\r\n");

		again = true;

		while (again)
		{
			play_recorded_sample(fnum-1);
			again = ask_yes_no("Do you want to listen it again?");
		}

		accepted = ask_yes_no("Are you satisfied with this sample?");

		if (!accepted) {
			again = ask_yes_no("Do you wish to record another sample? "
				"If you answer no the recorded sample will be discarded.");

			if (!again)
			{
				discard_recorded_sample(fnum-1);
				printf("Discarded!\r\n");
			}
		}
		else
		{
			printf("Sample accepted!\r\n");
		}
	}
}

/**
 * Implements the main loop that is executed whenever the program is in text
 * mode.
 * In this mode, the user can configure the system before starting the graphic
 * mode.
 */
static inline void terminal_mode()
{
char buffer[MAX_CHAR_BUFFER_SIZE];	// Buffer containing the inserted line
char command[MAX_CHAR_BUFFER_SIZE];	// Parsed command
char argument[MAX_CHAR_BUFFER_SIZE];// Parsed optional argument
bool start_graphic_mode = false;	// Used to exit this mode
int num_strings;					// The number of strings that have been
									// inserted by the user
int fnum;							// File number optionally specified by the
									// user
int err;

	printf("\r\n\r\nTerminal mode enabled.\r\n"
		"In this mode you can edit your opened files.\r\n"
		"Type help for a list of the available commands...\r\n");

	// While the program should keep running in terminal mode
	while(!main_state.quit && !start_graphic_mode)
	{
		// Print prompt
		printf("\r\n:");

		// Block reading a whole line
		fgets(buffer, sizeof(buffer), stdin);

		num_strings = sscanf(buffer, "%s %s", command, argument);

		if (num_strings < 1 || strlen(command) < 1)
		{
			// Empty line, do nothing, start loop again
		}
		else if (strcmp(command, "help") == 0)
		{
			cmd_help();
		}
		else if (strcmp(command, "pwd") == 0)
		{
			cmd_pwd();
		}
		else if (strcmp(command, "quit") == 0)
		{
			// Requested program termination, this will break the loop
			main_state.quit = true;
		}
		else if (strcmp(command, "open") == 0)
		{
			if (num_strings < 2)
				printf("Invalid command. Missing file name.\r\n");
			else
				cmd_open(argument);
		}
		else if (strcmp(command, "listen") == 0)
		{
			// Convert second argument to a number
			err = sscanf(argument, "%d", &fnum);

			if (err < 1)
				printf("Invalid command. Missing file number.\r\n");
			else
				cmd_listen(fnum);
		}
		else if (strcmp(command, "close") == 0)
		{
			// Convert second argument to a number
			err = sscanf(argument, "%d", &fnum);

			if (err < 1)
				printf("Invalid command. Missing file number.\r\n");
			else
				cmd_close(fnum);
		}
		else if (strcmp(command, "list") == 0)
		{
			cmd_list_audio_files();
		}
		else if (strcmp(command, "play") == 0)
		{
			// Requested switch from terminal to gtraphic mode. This will break
			// the loop.
			start_graphic_mode = true;
		}
		else if (strcmp(command, "record") == 0)
		{
			// Convert second argument to a number
			err = sscanf(argument, "%d", &fnum);

			if (err < 1)
				printf("Invalid command. Missing file number.\r\n");
			else
				cmd_record(fnum);
		}
		else
		{
			printf("Invalid command. Try again.\r\n");
		}
	}
}

//@}

/* ------------------------ TASKS HANDLING FUNCTIONS ------------------------ */

/**
 * @name Tasks handling private functions
 */
//@{

/**
 * Initializes and starts the GUI task, returning zero on success.
 */
static inline int start_gui_task()
{
	return
		ptask_short(
			&main_state.tasks[TASK_GUI],
			TASK_GUI_WCET,
			TASK_GUI_PERIOD,
			TASK_GUI_DEADLINE,
			GET_PRIO(TASK_GUI_PRIORITY),
			gui_task);
}

/**
 * Initializes and starts the UI task, returning zero on success.
 */
static inline int start_ui_task()
{
	return
		ptask_short(
			&main_state.tasks[TASK_UI],
			TASK_UI_WCET,
			TASK_UI_PERIOD,
			TASK_UI_DEADLINE,
			GET_PRIO(TASK_UI_PRIORITY),
			user_interaction_task);
}

/**
 * Initializes and starts the recording task, returning zero on success.
 */
static inline int start_microphone_task()
{
	return
		ptask_short(
			&main_state.tasks[TASK_MIC],
			TASK_MIC_WCET,
			TASK_MIC_PERIOD,
			TASK_MIC_DEADLINE,
			GET_PRIO(TASK_MIC_PRIORITY),
			microphone_task);
	//return 0;
}

/**
 * Initializes and starts the FFT task, returning zero on success.
 */
static inline int start_fft_task()
{
	return
		ptask_short(
			&main_state.tasks[TASK_FFT],
			TASK_FFT_WCET,
			TASK_FFT_PERIOD,
			TASK_FFT_DEADLINE,
			GET_PRIO(TASK_FFT_PRIORITY),
			fft_task);
}


/**
 * Initializes and starts the analyzer task, returning zero on success.
 * TODO: it may be necessary to start many of these
 */
static inline int start_analyzer_task()
{
	// TODO: This task should be divided between multiple tasks, one for each
	// opened file with an associated record trigger.

	/*
	return
		ptask_short(
			&main_state.tasks[TASK_ALS],
			TASK_ALS_WCET,
			TASK_ALS_PERIOD,
			TASK_ALS_DEADLINE,
			GET_PRIO(TASK_ALS_PRIORITY),
			analyzer_task);
	*/
	return 0;
}

/**
 * Aborts the task specified by the id. It is unsafe, since the task will leave
 * all data structures in a dirty condition, so it shall be called only when
 * closing the program if necessary. Also, it assumes that the task is in
 * cancelable state when called, see ptask_cancel().
 */
static inline void abort_task(int task_id)
{
	ptask_cancel(&main_state.tasks[task_id]);
	ptask_join(&main_state.tasks[task_id]);
}

/**
 * Initializes all the concurrent tasks in the system and starts them, one by
 * one. On error, it returns zero and the program should abort to avoid having
 * zombie tasks.
 */
static inline int initialize_tasks()
{
int err;

	main_state.tasks_terminate = false;

	err = start_gui_task();
	if (err) return err;

	err = start_ui_task();
	if (err) return err;

	err = start_microphone_task();
	if (err) return err;

	err = start_fft_task();
	if (err) return err;

	err = start_analyzer_task();
	return err;
}

/**
 * Blocks the thread execution to join all the active tasks.
 */
static inline void join_tasks()
{
// TODO: Update this function to reflect new tasks organization.

// int i;
	// for (i = 0; i < TASK_NUM; ++i)
	// 	ptask_join(&main_state.tasks[i]);

	ptask_join(&main_state.tasks[TASK_UI]);
	ptask_join(&main_state.tasks[TASK_GUI]);
	ptask_join(&main_state.tasks[TASK_MIC]);
}

//@}

/* -------------------------- MAIN THREAD HANDLING -------------------------- */

/**
 * @name Main thread handling functions
 */
//@{

/**
 * Forever loop for the main thread, until the graphic mode termination is
 * requested.
 */
static inline void main_wait()
{
	ptask_mutex_lock(&main_state.mutex);

	while(!main_state.tasks_terminate)
		ptask_cond_wait(&main_state.cond, &main_state.mutex);

	ptask_mutex_unlock(&main_state.mutex);

	// Wait for termination of all the tasks
	join_tasks();
}

/**
 * Initializes the program and the Allegro resources needed through all the
 * program life.
 * Returns zero on success, a non zero value otherwise.
 */
static inline int program_init()
{
int err;

#ifdef NDEBUG
	// Program has not been compiled for debug, so I can use real-time
	// scheduling
	err = ptask_set_scheduler(SCHED_FIFO); if(err) return err;
#else
	// Cannot debug using sudo privileges, so for debugging purposes I'll use
	// another scheduler
	err = ptask_set_scheduler(SCHED_OTHER); if(err) return err;
#endif

	// Allegro initialization
	err = allegro_init();
	if (err) return err;

	err = install_timer();
	if (err) return err;

	// Audio module initialization
	err = audio_init();
	if (err) return err;

	// Video module initialization
	err = video_init();
	if (err) return err;

	// Initializing semaphores
	err = ptask_mutex_init(&main_state.mutex);
	if (err) return err;
	err = ptask_cond_init(&main_state.cond);
	if (err) return err;

	return 0;
}

/**
 * Simply the main of the program.
 */
int main(int argc, char* argv[])
{
int err;

	// Read arguments from command line, if any
	err = read_arguments(argc, argv);
	if (err)
	{
		abort_on_error("Specified arguments are invalid or the directory "
			"specified does not exist.");
	}

	// Print current working directory on program initialization
	cmd_pwd();

	printf("Program initialization...\r\n");

	err = program_init();
	if (err)
		abort_on_error("Could not properly initialize the program.");

	while(!main_state.quit)
	{
		terminal_mode();

		if(!main_state.quit)
		{
			printf("Entering graphic mode...\r\n");

			printf("Starting concurrent tasks...\r\n");

			err = initialize_tasks();
			if (err)
				abort_on_error("Could not initialize concurrent tasks.");

			printf("Tasks started, entering the main wait mode...\r\n");

			main_wait();

			printf("Graphic mode terminated.\r\n");
		}
	}

	allegro_exit();

	return EXIT_SUCCESS;
}

//@}