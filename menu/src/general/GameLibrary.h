/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <dir.h>
#include <algorithm>
#include <cstring>
#include <strings.h>
#include <vector>
#include <deque>
#include <string>
#include <memory>

#include "Game.h"
#include "GameDatabase.h"
#include "INIParser.h"
#include "M64File.h"

/**
 * Each `ROMFile` and corresponding database entry has an associated memory
 * cost, so we have to draw the line somewhere.
 */
#define MAX_NUMBER_OF_FILES 1000

#define MAX_NUMBER_OF_DIRECTORIES 255
#define MAX_DIRECTORY_LENGTH 128

struct __attribute__((packed)) CacheHeader {
    /**
     * The two-character unique ID of the last selected game.
     * TODO: just read this from `PayloadData`?
     */
    char lastUniqueID[2];
    RegionCode lastRegionCode;

    /**
     * The number of ROM directories the user has chosen to load ROMs from.
     */
    uint8_t directoryCount;
    uint32_t empty2;
};

static_assert(sizeof(CacheHeader) == 8, "CacheHeader must be 8 bytes");

struct __attribute__((packed)) CacheResult {
    uint32_t hash = 0;
    uint32_t fileSize = 0;
    int16_t entryIndex = -1;
    uint8_t version = 0;
    uint32_t crc1 = 0;
};

class GameLibrary {
private:
    // Are we reading from the cache file?
    // This can get set to `false` mid-read if it's detected as stale
    bool readFromCache = false;

    // Number of populated entries in the global cache array after a load()
    int cachedFileCount = 0;

    // loadCache() returns early in non-release builds and on any read
    // failure, but loadGames() still matches against this to pick
    // lastLaunchedGame, so it must never hold garbage.
    CacheHeader temporaryCacheHeader = {};

    uint8_t directoryCount() const {
        return (uint8_t)std::min(romDirectoryPaths.size(), (size_t)MAX_NUMBER_OF_DIRECTORIES);
    }

public:
    GameDatabase& database;

    /**
     * Typically the root path of the filesystem.
     * rom:/sd/ on emulator
     * sd:/ on console
     */
    const char* path;

    /**
     * Should games be grouped by their unqiue game ID.
     */
    bool groupRetailGames = true;

    Game* lastLaunchedGame = nullptr;
    
    std::vector<Game> allGames;
    std::deque<M64File> allM64Files;
    
    std::vector<GameGroup> retailGroups;
    std::vector<GameGroup> homebrewGroups;
    std::vector<GameGroup> recentGroups;
    std::vector<GameGroup> favouriteGroups;

    std::vector<std::string> recentsPaths;
    std::vector<std::string> favouritesPaths;

    /**
     * A list of paths the user has chosen to load ROMs from.
     */
    std::vector<std::string> romDirectoryPaths;

    GameLibrary(GameDatabase& database, const char* path = nullptr);

    bool loadCache();
    bool writeCache();
    bool writeCacheHeader();

    void loadHistoryAndFavourites();

    /**
     * Persist the current recents and favourites back to menu/history.ini,
     * overwriting the file. Returns false if the path is unset or the file
     * can't be opened for writing.
     */
    bool saveHistoryAndFavourites();

    /**
     * Reload game list from the directories in `romDirectoryPaths`
     */
    void loadGames();

    /**
     * Traverse the filesystem looking for directories that contain at least
     * one file with a valid ROM file extension, starting in `path`
     */
    std::vector<std::string> findROMDirectories();

    void addToRecents(GameGroup gameGroup);

    void toggleFavourite(GameGroup gameGroup);

    void toggleRetailGroupings();
};
