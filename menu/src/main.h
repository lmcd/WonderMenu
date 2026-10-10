/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>

#include "general/BootSession.h"
#include "general/WarmResetState.h"

uint32_t get_rom_size(void);
uint32_t get_entry_point(void);

extern BootSession bootSession;
extern WarmResetState warmResetState;
