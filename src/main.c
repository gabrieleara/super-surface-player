#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <dirent.h>

#include <allegro.h>

#include "api/std_emu.h"
#include "api/ptask.h"

#include "constants.h"

#include "main.h"
#include "video.h"
#include "audio.h"


/* -----------------------------------------------------------------------------
 * GLOBAL VARIABLES
 * -----------------------------------------------------------------------------
 */

/* -----------------------------------------------------------------------------
 * DATA TYPES
 * -----------------------------------------------------------------------------
 */

 // Global state of the program
typedef struct __MAIN_STRUCT
{
	bool			tasks_terminate;// tells if concurrent tasks should stop
									// their execution
	bool			quit;			// tells if the program is shutting down
	bool			verbose;		// tells if the verbose flag has been set
	char			directory[MAX_DIRECTORY_LENGTH];
									// the specified directory where to search
									//for audio files

	ptask_t			tasks[TASK_NUM];// all the tasks data

	ptask_mutex_t	mutex;			// protects access to this data structure
	ptask_cond_t	cond;			// used to wake up main thread
} main_state_t;

// TODO: static?
main_state_t main_state =
{
	.tasks_terminate = false,
	.quit = false,
	.verbose = false,
	.directory = "",
	// TODO:
};

/* -----------------------------------------------------------------------------
 * UTILITY CALLBACKS
 * -----------------------------------------------------------------------------
 */

/*
 * Forcefully closes the program after displaying an error message.
 * Use only as last resource.
 */
void abort_on_error(char* message)
{
int len;

	if (message)
		printf("%s\r\n", message);

	/*if (allegro_error)
	{
		*/
	len = strnlen(allegro_error, ALLEGRO_ERROR_SIZE);

	if (len > 0)
		printf("Last allegro error is %s.\r\n", allegro_error);
	/*}*/

	exit(EXIT_FAILURE);
}

/* -----------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Checks an argument code. If the code is valid and it is the first time it is
 * specified enables that argument and returns zero, otherwise an error code is
 * returned.
 */
int _check_argument_code(char c)
{
int err = 0;

	switch (c)
	{
	case 'v':
		if(main_state.verbose)
			err = EINVAL;
		else
			main_state.verbose = true;
		break;

	// TODO: add other arguments

	default:
		// Unknown argument
		err = EINVAL;
	}

	return err;
}

bool _is_valid_directory(const char* path)
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

/*
 * Checks the command line arguments specified by the user.
 * Returns zero on success, an error code otherwise.
 */
int _read_arguments(int argc, char* argv[])
{
int			err = 0, i, len;
const char*	str;

	for (i = 1; i < argc && !err; ++i)
	{
		str = argv[i];
		if(str[0] == '-')
		{
			// It shall be a command line code specifier

			if(strlen(str) != 2)
				err = EINVAL;
			else
				err = _check_argument_code(str[1]);
		}
		else
		{
			// It shall be a directory specification

			len = strlen(str);

			if (strlen(main_state.directory)
				|| len >= MAX_DIRECTORY_LENGTH-1
				|| !_is_valid_directory(str))

				// Directory was already specified or is invalid
				err = EINVAL;
			else
			{
				strncpy(main_state.directory, str, MAX_DIRECTORY_LENGTH);

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


/* ------------------------- TERMINAL MODE COMMANDS ------------------------- */

/*
 * Displays all available commands in terminal mode.
 */
void cmd_help()
{
	printf("\r\n");
	printf("Available commands and their effects:\r\n\r\n");

	printf(" close\t<fnum>\tTo close an already opened audio/midi file.\r\n");
	printf(" help\t\tTo show this help.\r\n");
	printf(" list\t\tList all the opened audio/midi files.\r\n");
	printf(" listen\t<fnum>\tListen to the specified audio/midi file.\r\n");
	printf(" play\t\tTo start playing in windowed mode.\r\n");
	printf(" open\t<fname>\tTo open a new audio/midi file.\r\n");
	printf(" quit\t\tTo quit this program.\r\n");
	printf(" record\t<fnum>\tTo record an audio input that will trigger the file specified by the num.\r\n");

	printf("\r\n");

	printf(" \t\tTo see the <fnum> associated to a file, use the list command.\r\n");

	printf("\r\n");

	if (strlen(main_state.directory))
	{
		printf(" \t\tThe specified <fname> shall be an absolute path or a relative path to the directory\r\n \t\tspecified through the command line argument, which is:\r\n");
		printf("\t\t\t%s\r\n", main_state.directory);
	}
	else
		printf(" \t\tThe specified <fname> shall be an absolute path or a relative path to the current directory.\r\n");

	printf("\r\n");
}

/*
 * Opens an audio input file, being it a sample or a midi it doesn't matter.
 */
void cmd_open(char* filename)
{
char	buffer[MAX_CHAR_BUFFER_SIZE];
int		err = 0;

	if(filename[0] != '/')
	{
		strcpy(buffer, main_state.directory);
		strncat(buffer, filename, sizeof(buffer));
	}
	else
		// Absolute path
		strcpy(buffer, filename);

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
		printf("The requested file has been opened.\r\n");
}

void cmd_play(int fnum)
{
int err;

	// File numbers are specified as listed by the audio_list_fles,
	// but are played using zero based indexes.
	err = audio_play_file(fnum-1);

	if (err)
		printf("The specified file could not be played.\r\n");

}

void cmd_close(int fnum)
{
int err;

	// File numbers are specified as listed by the audio_list_fles,
	// but are played using zero based indexes.
	err = audio_close_file(fnum-1);

	if (err)
		printf("The specified file could not be closed.\r\n");
}

void terminal_mode()
{
char buffer[MAX_CHAR_BUFFER_SIZE];	// Buffer containing the inserted line
char command[MAX_CHAR_BUFFER_SIZE];	// Parsed command
char argument[MAX_CHAR_BUFFER_SIZE];// Parsed optional argument
bool start_graphic_mode = false;	// Used to exit this mode
int num_strings;
int fnum;							// File number optionally specified by the
									// user
int err;

	printf("\r\n\r\nTerminal mode enabled.\r\nIn this mode you can edit your opened files.\r\n");
	printf("Type help for a list of the available commands...\r\n");

	while(!main_state.quit && !start_graphic_mode)
	{
		printf("\r\n:");

		fgets(buffer, sizeof(buffer), stdin);

		num_strings = sscanf(buffer, "%s %s", command, argument);

		if (num_strings < 1)
			command[0] = '\0';

		if (strcmp(command, "help") == 0)
			cmd_help();
		else if (strcmp(command, "quit") == 0)
			main_state.quit = true;
		else if (strcmp(command, "open") == 0)
		{
			if (num_strings < 2)
				printf("Invalid command. Missing file name.\r\n");
			else
				cmd_open(argument);
		}
		else if (strcmp(command, "listen") == 0)
		{
			err = sscanf(argument, "%d", &fnum);

			if (err < 1)
				printf("Invalid command. Missing file number.\r\n");
			else
				cmd_play(fnum);
		}
		else if (strcmp(command, "close") == 0)
		{
			err = sscanf(argument, "%d", &fnum);

			if (err < 1)
				printf("Invalid command. Missing file number.\r\n");
			else
				cmd_close(fnum);
		}
		else if (strcmp(command, "list") == 0)
			audio_list_files();
		else if (strcmp(command, "play") == 0)
			start_graphic_mode = true;
		else if (strcmp(command, "record") == 0)
			;// TODO: record
		else if(strlen(command) < 1)
			; // Do nothing
		else
			printf("Invalid command. Try again.\r\n");

	}
}

/* -----------------------------------------------------------------------------
 * TASKS FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * The following functions initialize and start at the same time the relative
 * task.
 */

int start_gui_task()
{
	return
		ptask_short(
			&main_state.tasks[TASK_GUI],
			TASK_GUI_WCET,
			TASK_GUI_PERIOD,
			TASK_GUI_DEADLINE,
			GET_PRIO(TASK_GUI_PRIORITY),
			gui_task); // FIXME: define this task
}

int start_ui_task()
{
	return
		ptask_short(
			&main_state.tasks[TASK_UI],
			TASK_UI_WCET,
			TASK_UI_PERIOD,
			TASK_UI_DEADLINE,
			GET_PRIO(TASK_UI_PRIORITY),
			user_interaction_task); // FIXME: define this task
}

int start_microphone_task()
{
	// TODO: change period
	return
		ptask_short(
			&main_state.tasks[TASK_MIC],
			TASK_MIC_WCET,
			TASK_MIC_PERIOD,
			TASK_MIC_DEADLINE,
			GET_PRIO(TASK_MIC_PRIORITY),
			microphone_task); // FIXME: define this task
	return 0;
}

int start_fft_task()
{
	/*
	// TODO:
	return
		ptask_short(
			&main_state.tasks[TASK_FFT],
			TASK_FFT_WCET,
			TASK_FFT_PERIOD,
			TASK_FFT_DEADLINE,
			GET_PRIO(TASK_FFT_PRIORITY),
			fft_task); // FIXME: define this task
	*/
	return 0;
}


int start_analyzer_task()
{
	/* TODO:
	return
		ptask_short(
			&main_state.tasks[TASK_ALS],
			TASK_ALS_WCET,
			TASK_ALS_PERIOD,
			TASK_ALS_DEADLINE,
			GET_PRIO(TASK_ALS_PRIORITY),
			analyzer_task); // FIXME: define this task
	*/
	return 0;
}

/*
 * Aborts the task specified by the id. It is unsafe, since the task will leave
 * all data structures in a dirty condition, so it shall be called only when
 * closing the program if necessary.
 */
void abort_task(int task_id)
{
	// FIXME: unsafe, assumes that the task task_id is cancelable.
	ptask_cancel(&main_state.tasks[task_id]);
	ptask_join(&main_state.tasks[task_id]);
}

/*
 * Initializes all the concurrent tasks in the system and starts them, one by
 * one.
 */
int _initialize_tasks()
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

/*
 * Joins all the active tasks.
 */
void _join_tasks()
{
// TODO:
// int i;
	// for (i = 0; i < TASK_NUM; ++i)
	// 	ptask_join(&main_state.tasks[i]);

	ptask_join(&main_state.tasks[TASK_UI]);
	ptask_join(&main_state.tasks[TASK_GUI]);
	ptask_join(&main_state.tasks[TASK_MIC]);
}


/* -----------------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -----------------------------------------------------------------------------
 */

/*
 * Sends a signal to the main thread, waiting in the loop, which then will
 * terminate all the other tasks. This shall be called only in graphic mode.
 */
void main_terminate_tasks()
{
	ptask_mutex_lock(&main_state.mutex);

	main_state.tasks_terminate = true;
	ptask_cond_signal(&main_state.cond);

	ptask_mutex_unlock(&main_state.mutex);
}

/*
 * Forever loop for the main thread, until the graphic mode termination is
 * requested.
 */
void main_wait()
{
	ptask_mutex_lock(&main_state.mutex);

	while(!main_state.tasks_terminate)
		ptask_cond_wait(&main_state.cond, &main_state.mutex);

	ptask_mutex_unlock(&main_state.mutex);

	// Wait for termination of all the tasks
	_join_tasks();
}

/*
 * Returns wether tasks should terminate(gracefully) or keep going with their
 * computations.
 */
bool main_get_tasks_terminate()
{
bool ret;

	ptask_mutex_lock(&main_state.mutex);
	ret = main_state.tasks_terminate;
	ptask_mutex_unlock(&main_state.mutex);

	return ret;
}


/*
 * Initializes the program and the Allegro resources needed through all the
 * program life.
 * Returns zero on success, a non zero value otherwise.
 */
int program_init()
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






int main(int argc, char* argv[])
{
int err;
char cwd[1024];
	getcwd(cwd, sizeof(cwd));
	printf("Current working dir: %s\n", cwd);

	// Read arguments from command line, if any
	err = _read_arguments(argc, argv);
	if (err)
		abort_on_error("Specified arguments are invalid or the directory specified does not exist.");

	// Program initialization

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

			err = _initialize_tasks();
			if (err)
				abort_on_error("Could not initialize tasks");

			printf("Tasks started, entering the main wait mode...\r\n");

			main_wait();

			printf("Graphic mode terminated.\r\n");
		}
	}

	allegro_exit();

	return EXIT_SUCCESS;
}
