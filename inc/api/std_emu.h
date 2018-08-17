#ifndef STD_EMU_H
#define STD_EMU_H

// This file wants to emulate the behavior of the C99/C++ standard library,
// implementing a static_cast<T> operator.

// NOTICE however that the behavior of C-style casts is much dissimilar from
// static_cast<T> in C++ and sometimes more similar to a reinterpret_cast<T>,
// because for any struct it reinterprets bits of the given struct as bits of
// the destination one. It is safe however for primitive types conversions.

#define STATIC_CAST(type, expr) ((type)(expr))

// In C99, C++ bool type is defined as _BOOL. The following header file defines
// an alias for that type called (unsurprisingly) bool, which is recommended to
// be used for non-legagy code instead of _BOOL.
// #include <stdbool.h>



#endif
