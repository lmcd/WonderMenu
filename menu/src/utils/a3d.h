#ifndef UTILS_A3D_H__
#define UTILS_A3D_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file a3d.h
 * @brief Analogue 3D detection callable from C++.
 * @ingroup utils
 */

/**
 * @brief Detect if the running system is an Analogue 3D.
 *
 * Wraps libdragon's a3d_detect(), whose header lacks the extern "C" guard, so
 * C++ code including it looks for a mangled symbol that doesn't exist.
 *
 * @return true if running on an Analogue 3D.
 */
bool is_analogue_3d(void);

#ifdef __cplusplus
}
#endif

#endif
