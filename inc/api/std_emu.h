/**
 * @file std_emu.h
 * @brief Utility functions and macros
 *
 * @author Gabriele Ara
 * @date 2019/01/17
 *
 * Except the ones that shall be called from a single-thread environment,
 * functions are safe from a concurrency point of view.
 *
 */


#ifndef STD_EMU_H
#define STD_EMU_H

/**
 * Casts the given expression to the given type.
 *
 * While the objective of this macro was to obtain a behavior similar to C++
 * static_cast<T> operator, it shall be noted that the actual behavior of this
 * macro follows the one of the basic cast operator of the C lanuage, thus
 * resembling more the behavior of C++ reinterpret_cast<T>.
 *
 * In any case, it's a convenient macro that can be used to avoid problems
 * related to the somewhat ambiguity of the C cast operator, which when used
 * alone may lead to problems.
 *
 * As example, take
 * \code
 * long   a = 10L;
 * double b = 2.5;
 * int    c = (int) a * b;
 * \endcode
 *
 * The standard specifies that the cast operator has a precedence over other
 * operators, thus the previous operation is equivalent to the following one:
 * \code
 * int c = ((int)(a)) * b;
 * \endcode
 * which may not be what the programmer intended.
 *
 * That's why for casts I use the macro here defined like this:
 * \code
 * int c = STATIC_CAST(int, a * b);
 * \endcode
 * avoiding any ambiguity.
 */
#define STATIC_CAST(type, expr) ((type)(expr))

// In C99, C++ bool type is defined as _BOOL. The following header file defines
// an alias for that type called (unsurprisingly) bool, which is recommended to
// be used for non-legagy code instead of _BOOL.
// #include <stdbool.h>

#endif
