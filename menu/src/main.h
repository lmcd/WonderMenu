/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>

#include "general/BootSession.h"
#include "general/CartAllocator.h"
#include "general/WarmResetState.h"

extern BootSession bootSession;
extern CartAllocator cartAllocator;
extern WarmResetState warmResetState;
