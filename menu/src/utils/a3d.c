/**
 * @file a3d.c
 * @brief Analogue 3D detection callable from C++.
 * @ingroup utils
 */

#include <libdragon.h>

#include "a3d.h"

bool is_analogue_3d(void) {
    return a3d_detect();
}
