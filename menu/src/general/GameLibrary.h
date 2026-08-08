/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <dir.h>
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

#define MAX_NUMBER_OF_FILES 1000

struct __attribute__((packed)) CacheHeader {
    // The 2 character game ID (e.g. BK)
    char lastUniqueID[2];
    RegionCode lastRegionCode;
    uint8_t empty1;
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

public:
    GameDatabase& database;
    const char* path;

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

    GameLibrary(GameDatabase& database, const char* path = nullptr);

    void loadCache();
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
     * Reload game list from filesystem path
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
