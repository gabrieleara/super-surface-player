#ifndef __STD_EMU_H__
#define __STD_EMU_H__

// This file wants to emulate the behavior of the C99/C++ standard library,
// implementing a static cast operator and a bool data type.

#define STATIC_CAST(type, expr) ((type)(expr))

#define BOOL(expr) (!!(expr))

// Defnes a C equivalent bool class. However, please use the BOOL macro defined
// before to cast an expression to a boolean value, since values greater than 1
// casted to an enum value result to undefined behavior.
#ifndef __cplusplus
	typedef enum {false, true} bool;
#endif

#endif