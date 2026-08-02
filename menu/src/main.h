/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>

#include "general/GameLaunchSession.h"

uint32_t get_rom_size(void);
uint32_t get_entry_point(void);

extern GameLaunchSession gameLaunchSession;
