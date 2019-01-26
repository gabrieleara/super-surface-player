/**
 * @file main.h
 * @brief Main program public functions and data types
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * The main module is reponsible for the setup of the application (in particular
 * when executed in text mode) and to manage tasks once the graphical mode is
 * started.
 *
 */


#ifndef MAIN_H
#define MAIN_H

// -----------------------------------------------------------------------------
//                             PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * Returns true if the verbose flag is set.
 */
extern bool verbose();

/**
 * Returns true if the wcet_analysis flag is set.
 */
extern bool wcet_analysis();

/**
 * Returns the path of the current working directory.
 */
extern char* working_directory();


/**
 * Forcefully closes the program after displaying an error message.
 * Use only as last resource.
 */
extern void abort_on_error(char* message);

/**
 * Requests the terminayion of the graphical mode by signaling the main thread.
 * This function shall be called in graphic mode only.
 */
extern void main_terminate_tasks();

/**
 * Returns true if the graphical mode termination has been requested, thus
 * concurrent task should terminate (gracefully) their execution.
 */
extern bool main_get_tasks_terminate();

#endif
