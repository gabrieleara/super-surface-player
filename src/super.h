#ifndef SUPER_H
#define SUPER_H

#include <stdlib.h>
#include <stdio.h>

#ifdef SUPER_DEBUG
	// Enabling asserts when debugging
	#include <assert.h>
	#define NDEBUG
#endif

#include <allegro.h>

#include "constants.h"

#include "api/std_emu.h"
#include "api/ptask.h"

#endif