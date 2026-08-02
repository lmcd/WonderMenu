/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "Game.h"

#include <filesystem>
#include <cstring>
#include <cstdio>

Game::Game(ROMFile& romFile, GameDatabase::Entry* databaseEntry) :romFile(romFile), databaseEntry(databaseEntry) {}

Game::~Game() {
    // The reference is bound to a heap ROMFile the Game took ownership of at
    // construction, so it still has to be freed here.
    delete &romFile;

    // Note: databaseEntry is owned by GameDatabase, don't delete
}

const char* Game::title() const {
    // Prefer database title if available
    if (databaseEntry && databaseEntry->title[0] != '\0') {
        return databaseEntry->title;
    }

    // Last resort: return filename. Cached on the Game rather than built into a
    // local -- a local's buffer is freed when this returns, leaving the caller
    // with a dangling pointer.
    if (!romFile.path.empty()) {
        if (fallbackTitle.empty()) {
            fallbackTitle = std::filesystem::path(romFile.path).filename().string();
        }

        return fallbackTitle.c_str();
    }

    return "Unknown";
}

const char* Game::developer() const {
    // Return database developer if available
    if (databaseEntry && databaseEntry->developer[0] != '\0') {
        return databaseEntry->developer;
    }

    return "Unknown";
}
