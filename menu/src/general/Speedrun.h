/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "general/M64File.h"
#include "general/SpeedrunDatabase.h"

struct Speedrun {
    M64File& m64File;
    SpeedrunEntry databaseEntry;
};
