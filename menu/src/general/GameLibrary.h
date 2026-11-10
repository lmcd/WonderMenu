/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <algorithm>
#include <vector>
#include <deque>
#include <string>

#include "Game.h"
#include "GameDatabase.h"
#include "M64File.h"

/**
 * Each `ROMFile` and corresponding database entry has an associated memory
 * cost, so we have to draw the line somewhere.
 */
#define MAX_NUMBER_OF_ROM_FILES 1000

#define MAX_NUMBER_OF_DIRECTORIES 255
#define MAX_DIRECTORY_LENGTH 128

#define MAX_NUMBER_OF_RECENTS 16
#define MAX_NUMBER_OF_FAVOURITES 16

struct __attribute__((packed)) CacheHeader {
    static constexpr uint8_t CURRENT_VERSION = 1;

    // Identifies the file as a WonderMenu cache
    char magic[2] = {'W', 'M'};

    // Bumped whenever the cache's layout changes, so an older cache is
    // rebuilt rather than misread.
    uint8_t version = CURRENT_VERSION;

    // The number of ROM directories the user has chosen to load ROMs from.
    uint8_t directoryCount;

    // The last-known number of ROM files. 
    uint16_t romFileCount;

    // The last-known number of M64 files.
    uint16_t m64FileCount;

    bool isValid() const {
        return magic[0] == 'W' && magic[1] == 'M' && version == CURRENT_VERSION;
    }
};

static_assert(sizeof(CacheHeader) == 8, "CacheHeader must be 8 bytes");

struct __attribute__((packed)) ROMCacheResult {
    uint32_t hash = 0;
    uint32_t fileSize = 0;
    int16_t entryIndex = -1;
    uint8_t version = 0;
    uint32_t crc1 = 0;
    uint8_t reserved = 0;
};

static_assert(sizeof(ROMCacheResult) == 16, "ROMCacheResult must be 16 bytes");

// What M64File::loadHeader() found in a .m64 file, so it needn't be read again
struct __attribute__((packed)) M64CacheResult {
    uint32_t hash = 0;
    uint32_t fileSize = 0;
    uint32_t romCRC = 0;
    uint32_t crc = 0;
    M64File::AuthoringProgram authoringProgram = {};
    uint8_t controllerCount = 0;

    // False if loadHeader() failed, so the file is skipped again
    bool isLoaded = false;

    uint8_t reserved[2] = {0};
};

static_assert(sizeof(M64CacheResult) == 24, "M64CacheResult must be 24 bytes");

class GameLibrary {
private:
    // Are we reading from the cache file?
    // This can get set to `false` mid-read if it's detected as stale
    bool readFromCache = false;

    // As `readFromCache`, for the M64 files
    bool readM64sFromCache = false;

    // Number of populated entries in `romCache` after a load()
    int cachedROMFileCount = 0;

    // Number of populated entries in `m64Cache` after loadM64Files()
    int cachedM64FileCount = 0;

    // Only needed while the library loads, from loadCache() to the end of
    // loadM64Files(), so they're freed then rather than held for the whole
    // session
    std::vector<ROMCacheResult> romCache;
    std::vector<M64CacheResult> m64Cache;

    void allocateCache() {
        if (romCache.empty()) {
            romCache.resize(MAX_NUMBER_OF_ROM_FILES);
        }
    }

    // loadCache() returns early in non-release builds and on any read
    // failure, but loadGames() still reads directoryCount back out of this,
    // so it must never hold garbage.
    CacheHeader temporaryCacheHeader = {};

    uint8_t directoryCount() const {
        return (uint8_t)std::min(romDirectoryPaths.size(), (size_t)MAX_NUMBER_OF_DIRECTORIES);
    }

    // Adds `relativePath` (relative to `path`, empty for the root) to
    // `directoryPaths` if it holds ROMs, then searches its subdirectories down
    // to `depth` more levels
    void findROMDirectories(const std::string& relativePath, int depth, std::vector<std::string>& directoryPaths);

public:
    GameDatabase& database;

    /**
     * Typically the root path of the filesystem.
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

    std::vector<Game*> speedrunGames;

    /**
     * A list of paths the user has chosen to load ROMs from.
     */
    std::vector<std::string> romDirectoryPaths;

    GameLibrary(GameDatabase& database, const char* path = nullptr);

    void createDirectories();

    bool loadCache();
    bool writeCache();

    void loadHistoryAndFavourites();

    /**
     * Persist the current recents and favourites back to menu/history.ini,
     * overwriting the file. Returns false if the path is unset or the file
     * can't be opened for writing.
     */
    bool saveHistoryAndFavourites();

    void warmClusterOffsets();

    /**
     * Reload game list from the directories in `romDirectoryPaths`
     */
    void loadGames();

    /**
     * Load the .m64 files in the speedruns directory, attaching each to its
     * game. Must follow loadGames(), as it writes the cache for both.
     */
    void loadM64Files();

    /**
     * Write the inputs the payload recorded into cart SDRAM out to a new .m64
     * file in the speedruns directory.
     */
    bool writeRecordedSpeedrun(Game& game, uint32_t inputsAddress, uint32_t sampleCount, uint32_t controllerCount);

    /**
     * Traverse the filesystem looking for directories that contain at least
     * one file with a valid ROM file extension, starting in `path`.
     *
     * @param depth How many levels of subdirectories below `path` to search.
     *              0 searches only `path` itself, 1 also its immediate
     *              subdirectories, and so on.
     * @return The directories relative to `path`, with no trailing slash
     *         ("" for `path` itself, "A/B" for a nested one). Each directory
     *         comes before its subdirectories, and siblings are sorted.
     */
    std::vector<std::string> findROMDirectories(int depth = 1);

    void addToRecents(GameGroup gameGroup);

    void toggleFavourite(GameGroup gameGroup);

    void toggleRetailGroupings();
};
