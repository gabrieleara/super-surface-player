#ifndef STD_EMU_H
#define STD_EMU_H

// This file wants to emulate the behavior of the C99/C++ standard library,
// implementing a static cast operator and including stdbool header.

// In C99, C++ bool type is defined as _BOOL. The following header file defines
// an alias for that type called (unsurprisingly) bool, which is recommended to
// be used for non-legagy code instead of _BOOL.
#include <stdbool.h>

#define STATIC_CAST(type, expr) ((type)(expr))

#endif
