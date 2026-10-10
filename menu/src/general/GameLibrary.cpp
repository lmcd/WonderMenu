/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "DirectoryScanner.h"
#include "INIParser.h"
#include "GameLibrary.h"
#include "general/M64File.h"
#include "utils/fs.h"
#include "utils/utils.h"

CacheResult cache[MAX_NUMBER_OF_FILES] = {0};

static GameIDMap<int, -1> entryIndexMap;

GameLibrary::GameLibrary(GameDatabase& database, const char* path) :
    database(database),
    path(path) {
    allGames.reserve(1000);
    retailGroups.reserve(1000);
    homebrewGroups.reserve(64);
    favouriteGroups.reserve(64);
    recentGroups.reserve(64);
}

void GameLibrary::createDirectories() {
    // TODO: use `path`
    // TODO: make these constants somewhere
    directory_create("sd:/menu");
    directory_create("sd:/speedruns");
    directory_create("sd:/saves");
}

bool GameLibrary::loadCache() {
    romDirectoryPaths.clear();

    char cachePath[512];
    snprintf(cachePath, sizeof(cachePath), "sd:/cache.bin");

    FIL cacheFile = {};

    if (f_open(&cacheFile, strip_fs_prefix(cachePath), FA_READ) != FR_OK) {
        debugf("[GameLibrary] Failed to open cache file: %s\n", cachePath);
        readFromCache = false;
        return false;
    }

    size_t fileSize = f_size(&cacheFile);

    debugf("[GameLibrary] Found cache file. Size: %zu\n", fileSize);

    UINT bytesRead = 0;

    if (f_read(&cacheFile, &temporaryCacheHeader, sizeof(CacheHeader), &bytesRead) != FR_OK || bytesRead != sizeof(CacheHeader)) {
        debugf("[GameLibrary] Failed to read cache header\n");
        readFromCache = false;
        f_close(&cacheFile);
        return false;
    }

    int directoryCount = temporaryCacheHeader.directoryCount;

    if (directoryCount > 0) {
        std::vector<char> directoryBuffer(directoryCount * MAX_DIRECTORY_LENGTH);

        if (f_read(&cacheFile, directoryBuffer.data(), directoryBuffer.size(), &bytesRead) != FR_OK || bytesRead != directoryBuffer.size()) {
            debugf("[GameLibrary] Failed to read %i cache directories\n", directoryCount);

            readFromCache = false;
            f_close(&cacheFile);
            return false;
        }

        for (int i = 0; i < directoryCount; i++) {
            char* directoryPath = directoryBuffer.data() + (i * MAX_DIRECTORY_LENGTH);
            directoryPath[MAX_DIRECTORY_LENGTH - 1] = '\0';

            romDirectoryPaths.push_back(directoryPath);
        }
    }

    debugf("[GameLibrary] Read %i ROM directories from cache\n", directoryCount);

    size_t entriesOffset = sizeof(CacheHeader) + (directoryCount * MAX_DIRECTORY_LENGTH);

    if (fileSize < entriesOffset) {
        debugf("[GameLibrary] Cache file is shorter than its header claims\n");

        romDirectoryPaths.clear();
        readFromCache = false;
        f_close(&cacheFile);
        return false;
    }

    // Don't read past the bounds of the cache array
    size_t bytesToRead = std::min(fileSize - entriesOffset, sizeof(cache));

    if (f_read(&cacheFile, cache, bytesToRead, &bytesRead) != FR_OK || bytesRead != bytesToRead) {
        debugf("[GameLibrary] Failed to read cache file (read %u of %zu bytes)\n", (unsigned)bytesRead, bytesToRead);
        f_close(&cacheFile);
        return false;
    }

    f_close(&cacheFile);

    readFromCache = true;

    return !romDirectoryPaths.empty();
}

bool GameLibrary::writeCache() {
    char cachePath[512];
    snprintf(cachePath, sizeof(cachePath), "sd:/cache.bin");

    FILE* cacheFile = fopen(cachePath, "wb");

    if (!cacheFile) {
        debugf("[GameLibrary] Failed to open cache file for writing: %s\n", cachePath);

        return false;
    }

    CacheHeader header = {};
    header.romFileCount = cachedFileCount;
    header.directoryCount = directoryCount();

    if (fwrite(&header, 1, sizeof(CacheHeader), cacheFile) != sizeof(CacheHeader)) {
        debugf("[GameLibrary] Failed to write blank cache header\n");
        fclose(cacheFile);
        return false;
    }

    for (int i = 0; i < header.directoryCount; i++) {
        char directoryPath[MAX_DIRECTORY_LENGTH] = {};
        snprintf(directoryPath, sizeof(directoryPath), "%s", romDirectoryPaths[i].c_str());

        if (fwrite(directoryPath, 1, sizeof(directoryPath), cacheFile) != sizeof(directoryPath)) {
            debugf("[GameLibrary] Failed to write cache directory %i\n", i);
            fclose(cacheFile);
            return false;
        }
    }

    size_t entriesToWrite = std::min((size_t)cachedFileCount, (size_t)MAX_NUMBER_OF_FILES);

    size_t entriesWritten = fwrite(cache, sizeof(CacheResult), entriesToWrite, cacheFile);

    fclose(cacheFile);

    if (entriesWritten != entriesToWrite) {
        debugf("[GameLibrary] Failed to write cache file (wrote %zu of %zu entries)\n", entriesWritten, entriesToWrite);

        return false;
    }
    else {
        debugf("[GameLibrary] Wrote cache file. Entries: %i\n", entriesWritten);

        return true;
    }
}

void GameLibrary::loadHistoryAndFavourites() {
    if (!path) {
        return;
    }

    char iniPath[512];
    snprintf(iniPath, sizeof(iniPath), "%smenu/history.ini", path);

    debugf("[GameLibrary] Loading INI file: %s\n", iniPath);

    INIParser parser(iniPath);
    INIParser::Entry iniEntry;
    
    parser.beginSection("history");

    while (parser.read(iniEntry)) {
        if (iniEntry.primaryPath.empty()) {
            continue;
        }

        recentsPaths.push_back(iniEntry.primaryPath);
    }

    parser.beginSection("favorite");

    while (parser.read(iniEntry)) {
        if (iniEntry.primaryPath.empty()) {
            continue;
        }

        favouritesPaths.push_back(iniEntry.primaryPath);
    }

    parser.close();

    debugf("[GameLibrary] Loaded %i Recent Paths\n", recentsPaths.size());
    debugf("[GameLibrary] Loaded %i Favourite Paths\n", favouritesPaths.size());
}

bool GameLibrary::saveHistoryAndFavourites() {
    if (!path) {
        return false;
    }

    char iniPath[512];
    snprintf(iniPath, sizeof(iniPath), "%smenu/history.ini", path);

    debugf("[GameLibrary] Saving INI file: %s\n", iniPath);

    FILE* file = fopen(iniPath, "w");

    if (!file) {
        debugf("[GameLibrary] Failed to open INI file for writing: %s\n", iniPath);
        return false;
    }

    // See `INIParser.h` for file format example
    auto writeSection = [file](const char* section, const std::vector<GameGroup>& groups) {
        fprintf(file, "[%s]\n", section);

        int index = 0;

        for (const GameGroup& group : groups) {
            const Game* game = group.preferredGame();

            if (game == nullptr) {
                continue;
            }

            fprintf(file, "%d_primary_path=%s\n", index, game->romFile.path.c_str());
            fprintf(file, "%d_secondary_path=\n", index);
            fprintf(file, "%d_type=1\n", index);

            index++;
        }
    };

    writeSection("history", recentGroups);
    fprintf(file, "\n");
    writeSection("favorite", favouriteGroups);

    fclose(file);

    return true;
}

void GameLibrary::addToRecents(GameGroup gameGroup) {
    Game* game = gameGroup.preferredGame();

    recentGroups.erase(
        std::remove(recentGroups.begin(), recentGroups.end(), gameGroup),
        recentGroups.end()
    );

    recentGroups.emplace(recentGroups.begin(), *game);

    saveHistoryAndFavourites();
}

// TODO: should be reference
// TODO: should be `Game`
void GameLibrary::toggleFavourite(GameGroup gameGroup) {
    Game* game = gameGroup.preferredGame();
    game->isFavourite = !game->isFavourite;

    if (!game->isFavourite) {
        auto it = std::find(favouriteGroups.begin(), favouriteGroups.end(), gameGroup);
        if (it != favouriteGroups.end()) {
            favouriteGroups.erase(it);
        }
    }
    else {
        favouriteGroups.emplace(favouriteGroups.begin(), *game);
    }

    saveHistoryAndFavourites();
}

void GameLibrary::toggleRetailGroupings() {
    groupRetailGames = !groupRetailGames;

    retailGroups.clear();

    static GameIDMap<int, -1> entryIndexMap2;
    // Reset to the -1 "unset" sentinel (memset 0 would leave cells reading as a
    // valid index 0, sending the first game down the .at() path on empty data).
    entryIndexMap2.reset();

    int entryIndex = 0;

    for (Game& game : allGames) {
        if (game.hasHomebrewGameCode()) {
            continue;
        }

        ROMFile& romFile = game.romFile;

        char* uniqueID = romFile.uniqueID;
        int foundRetailEntryIndex = entryIndexMap2[uniqueID];

        if (!groupRetailGames) {
            foundRetailEntryIndex = -1;
        }

        if (foundRetailEntryIndex == -1) {
            entryIndexMap2[uniqueID] = entryIndex;

            retailGroups.emplace_back(game);
            entryIndex++;
        }
        else {
            GameGroup& gameGroup = retailGroups.at(foundRetailEntryIndex);
            gameGroup.addGame(game);
        }
    }

    // Sort retail games alphabetically by display title (case-insensitive)
    std::sort(retailGroups.begin(), retailGroups.end(), [](const GameGroup& a, const GameGroup& b) {
        return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
    });
}

std::vector<std::string> GameLibrary::findROMDirectories() {
    std::vector<std::string> romDirectoryPaths;
    std::vector<std::string> subdirectoryNames;

    if (path == nullptr) {
        return romDirectoryPaths;
    }

    dir_t entry;

    if (dir_findfirst(path, &entry) != 0) {
        debugf("[GameLibrary] No folders found in %s\n", path);
        return romDirectoryPaths;
    }

    bool rootHasROMs = false;

    do {
        if (entry.d_type == DT_DIR) {
            if (entry.d_name[0] != '.') {
                subdirectoryNames.push_back(entry.d_name);
            }
        }
        else if (!rootHasROMs && ROMFile::hasROMExtension(entry.d_name)) {
            rootHasROMs = true;
        }
    } while (dir_findnext(path, &entry) == 0);

    std::sort(subdirectoryNames.begin(), subdirectoryNames.end(), [](const std::string& a, const std::string& b) {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    });

    if (rootHasROMs) {
        romDirectoryPaths.push_back("");
    }

    for (const std::string& subdirectoryName : subdirectoryNames) {
        char subdirectoryPath[512];
        snprintf(subdirectoryPath, sizeof(subdirectoryPath), "%s%s/", path, subdirectoryName.c_str());

        if (dir_findfirst(subdirectoryPath, &entry) != 0) {
            continue;
        }

        bool hasROMs = false;

        do {
            if (!hasROMs && entry.d_type != DT_DIR && ROMFile::hasROMExtension(entry.d_name)) {
                hasROMs = true;
            }
        } while (dir_findnext(subdirectoryPath, &entry) == 0);

        if (hasROMs) {
            romDirectoryPaths.push_back(subdirectoryName);
        }
    }

    debugf("[GameLibrary] Found %i ROM folders in %s\n", romDirectoryPaths.size(), path);

    return romDirectoryPaths;
}

void GameLibrary::warmClusterOffsets() {
    #ifndef N64
    return;
    #endif

    std::vector<GameDatabase::Entry*> entries;
    entries.reserve(allGames.size());

    for (Game& game : allGames) {
        if (game.databaseEntry != nullptr) {
            entries.push_back(game.databaseEntry);
        }
    }

    // Sort entries by tile offset, as seeking the filesystem from left
    // to right is faster
    std::sort(entries.begin(), entries.end(),
        [](const GameDatabase::Entry* a, const GameDatabase::Entry* b) {
            return a->smallLabelTileIndex < b->smallLabelTileIndex;
        }
    );

    database.warmClusterOffsets(entries);
}

void GameLibrary::loadGamesFast() {
    retailGroups.clear();

    entryIndexMap.reset();

    int fileIndex = -1;
    int entryIndex = 0;

    database.loadEntriesIntoMemory();

    // No directory scan at all: every entry is rebuilt from what the cache
    // already recorded, opening each ROM from its start cluster instead of a path
    int romFileCount = temporaryCacheHeader.romFileCount;

    FATFS *fs; WORD id;
    f_getfsid("0:/", &fs, &id);

    // The cache records a djb2 hash of each ROM's filename rather than its path,
    // so recents and favourites are matched by hashing their own filename the
    // same way. Hashed up front: otherwise it would be every path, every entry.
    auto filenameHash = [](const std::string& romPath) {
        size_t separator = romPath.rfind('/');

        const char* filename = (separator == std::string::npos)
            ? romPath.c_str()
            : romPath.c_str() + separator + 1;

        return GameDatabase::djb2((const unsigned char*)filename, strlen(filename));
    };

    std::vector<uint32_t> recentsHashes;
    std::vector<uint32_t> favouritesHashes;

    recentsHashes.reserve(recentsPaths.size());
    favouritesHashes.reserve(favouritesPaths.size());

    for (const std::string& recentPath : recentsPaths) {
        recentsHashes.push_back(filenameHash(recentPath));
    }

    for (const std::string& favouritePath : favouritesPaths) {
        favouritesHashes.push_back(filenameHash(favouritePath));
    }

    for (int i = 0; i < romFileCount; i++) {
        const CacheResult& cacheResult = cache[i];

        fileIndex++;

        ROMFile* romFile = new ROMFile("", fileIndex);

        // Lets the ROM be opened with no directory lookup
        romFile->fileObject.id = id;
        romFile->fileObject.fs = fs;
        romFile->fileObject.sclust = cacheResult.sclust;
        romFile->fileObject.objsize = cacheResult.fileSize;

        romFile->size = cacheResult.fileSize;
        romFile->version = cacheResult.version;
        romFile->crc1 = cacheResult.crc1;

        GameDatabase::Entry* databaseEntry = nullptr;

        if (cacheResult.entryIndex != -1) {
            databaseEntry = database.readEntryAtIndex(cacheResult.entryIndex);
        }

        Game& lastGame = allGames.emplace_back(*romFile, databaseEntry);

        if (databaseEntry != nullptr) {
            romFile->uniqueID[0] = databaseEntry->uniqueID[0];
            romFile->uniqueID[1] = databaseEntry->uniqueID[1];
            romFile->regionCode = databaseEntry->regionCode;

            if (databaseEntry->hasSpeedrunDatabase()) {
                speedrunGames.push_back(&lastGame);
            }
        }

        char* uniqueID = romFile->uniqueID;

        int foundRetailEntryIndex = entryIndexMap[uniqueID];

        if (foundRetailEntryIndex == -1) {
            bool hasHomebrewGameCode = romFile->hasHomebrewGameCode();

            if (hasHomebrewGameCode || databaseEntry == nullptr) {
                // Version field  for homebrew roms can't be trusted, so keep it a 1
                lastGame.romFile.version = 1;

                homebrewGroups.emplace_back(lastGame);
            }
            else {
                entryIndexMap[uniqueID] = entryIndex;

                retailGroups.emplace_back(lastGame);
                entryIndex++;
            }
        }
        else {
            GameGroup& gameGroup = retailGroups.at(foundRetailEntryIndex);
            gameGroup.addGame(lastGame);
        }

        if (std::find(recentsHashes.begin(), recentsHashes.end(), cacheResult.hash) != recentsHashes.end()) {
            recentGroups.emplace_back(lastGame);
        }

        if (std::find(favouritesHashes.begin(), favouritesHashes.end(), cacheResult.hash) != favouritesHashes.end()) {
            lastGame.isFavourite = true;

            favouriteGroups.emplace_back(lastGame);
        }
    }

    database.releaseEntriesFromMemory();

    // Sort retail games alphabetically by display title (case-insensitive)
    std::sort(retailGroups.begin(), retailGroups.end(), [](const GameGroup& a, const GameGroup& b) {
        return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
    });

    // Sort homebrew games alphabetically by display title (case-insensitive)
    std::sort(homebrewGroups.begin(), homebrewGroups.end(), [](const GameGroup& a, const GameGroup& b) {
        return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
    });

    auto orderIn = [](const std::vector<std::string>& paths, const GameGroup& group) {
        auto it = std::find(paths.begin(), paths.end(), group.preferredGame()->romFile.path);
        return it != paths.end() ? (int)std::distance(paths.begin(), it) : INT_MAX;
    };

    // Sort recent games by the order they appear in the INI file
    std::sort(recentGroups.begin(), recentGroups.end(), [&](const GameGroup& a, const GameGroup& b) {
        return orderIn(recentsPaths, a) < orderIn(recentsPaths, b);
    });

    // Sort favourite games by the order they appear in the INI file
    std::sort(favouriteGroups.begin(), favouriteGroups.end(), [&](const GameGroup& a, const GameGroup& b) {
        return orderIn(favouritesPaths, a) < orderIn(favouritesPaths, b);
    });

    // The most recently launched game is the first entry in recentGroups, which
    // the sort above put in INI order. Left null when there are no recents.
    lastLaunchedGame = recentGroups.empty() ? nullptr : recentGroups.front().preferredGame();

    debugf("[GameLibrary] File count %i\n", fileIndex);
    debugf("[GameLibrary] Loaded %i Retail Games\n", retailGroups.size());
    debugf("[GameLibrary] Loaded %i Homebrew Games\n", homebrewGroups.size());
    debugf("[GameLibrary] Loaded %i Recent Games\n", recentGroups.size());
    debugf("[GameLibrary] Loaded %i Favourite Games\n", favouriteGroups.size());
}

void GameLibrary::loadGames() {
    createDirectories();
    
    retailGroups.clear();

    entryIndexMap.reset();

    DirectoryScanner scanner;

    int fileIndex = -1;
    int entryIndex = 0;

    // Move database entires from cart space into memory for quicker retrieval.
    // NOTE: this suprisingly only saves about 22ms of load time for a library
    // of 900 games. Hmmm...
    database.loadEntriesIntoMemory();

    // Each directory is relative to `path` and carries no trailing slash (the
    // root itself is an empty string)
    for (const std::string& directoryPath : romDirectoryPaths) {
        char scanPath[512];

        if (directoryPath.empty()) {
            snprintf(scanPath, sizeof(scanPath), "%s", path);
        }
        else {
            snprintf(scanPath, sizeof(scanPath), "%s%s/", path, directoryPath.c_str());
        }

        debugf("[GameLibrary] Reading ROM files in %s\n", scanPath);

        // Skip directories that are empty or don't exist
        if (!scanner.open(scanPath)) {
            debugf("[GameLibrary] No ROM files found in %s\n", scanPath);
            continue;
        }

        // Process the first entry and continue with the rest
        do {
            // Skip directories
            if (scanner.isDirectory) {
                continue;
            }

            const char* filename = scanner.name;
            size_t filenameLength = scanner.nameLength;

            // Skip files without a valid extension (e.g. .z64)
            if (!ROMFile::hasROMExtension(filename, filenameLength)) {
                continue;
            }

            // The cache is a fixed-size array, so stop rather than run past it
            if (fileIndex + 1 >= MAX_NUMBER_OF_FILES) {
                debugf("[GameLibrary] Reached the %i file limit\n", MAX_NUMBER_OF_FILES);
                break;
            }

            // Build full path. Sized so scanPath (up to 511) + d_name (up to
            // 255) can't truncate, keeping -Werror=format-truncation happy.
            char fullPath[768];
            snprintf(fullPath, sizeof(fullPath), "%s%s", scanPath, filename);

            ROMFile* romFile = new ROMFile(fullPath, fileIndex);

            // Lets loadHeader() below open the file with no directory lookup
            romFile->fileObject = scanner.fileObject;

            GameDatabase::Entry* databaseEntry = nullptr;

            uint32_t hash = GameDatabase::djb2((const unsigned char*)filename, filenameLength);
            uint32_t fileSize = scanner.size;

            // Set the ROM size here so it is populated on the cache-hit path too;
            // loadHeader() (cache-miss only) would otherwise be the sole place it is
            // set, leaving romFile->size uninitialised for cached games.
            romFile->size = fileSize;

            fileIndex++;

            CacheResult cacheResult = cache[fileIndex];

            if (readFromCache) {
                if (cacheResult.hash != hash || cacheResult.fileSize != fileSize) {
                    readFromCache = false;

                    debugf("[GameLibrary] Hash mismatch for file: %s\n", filename);
                    debugf("[GameLibrary] Abandoning cache.\n");
                }
            }

            if (readFromCache) {
                if (cacheResult.entryIndex != -1) {
                    databaseEntry = database.readEntryAtIndex(cacheResult.entryIndex);
                }

                romFile->version = cacheResult.version;
                romFile->crc1 = cacheResult.crc1;

                cache[fileIndex].entryIndex = cacheResult.entryIndex;
                cache[fileIndex].version = cacheResult.version;
                cache[fileIndex].crc1 = cacheResult.crc1;
                cache[fileIndex].sclust = cacheResult.sclust;
            }
            else {
                cache[fileIndex] = CacheResult(hash, fileSize, 0, 0);
                
                if (romFile->loadHeader()) {
                    databaseEntry = database.entryForROMFile(romFile);

                    cache[fileIndex].entryIndex = (databaseEntry == nullptr) ? -1 : databaseEntry->entryIndex;
                    cache[fileIndex].version = romFile->version;
                    cache[fileIndex].crc1 = romFile->crc1;
                    cache[fileIndex].sclust = romFile->fileObject.sclust;
                } else {
                    debugf("[GameLibrary] Failed to load ROM file: %s\n", filename);

                    // Failed to load, clean up
                    delete romFile;

                    cache[fileIndex].entryIndex = -1;
                    cache[fileIndex].version = 0;
                    cache[fileIndex].crc1 = 0;
                    cache[fileIndex].sclust = 0;

                    continue;
                }
            }

            if (databaseEntry != nullptr) {
                romFile->uniqueID[0] = databaseEntry->uniqueID[0];
                romFile->uniqueID[1] = databaseEntry->uniqueID[1];
                romFile->regionCode = databaseEntry->regionCode;
            }

            Game& lastGame = allGames.emplace_back(*romFile, databaseEntry);

            if (databaseEntry != nullptr) {
                if (databaseEntry->hasSpeedrunDatabase()) {
                    speedrunGames.push_back(&lastGame);
                }
            }

            char* uniqueID = romFile->uniqueID;

            int foundRetailEntryIndex = entryIndexMap[uniqueID];

            if (foundRetailEntryIndex == -1) {
                bool hasHomebrewGameCode = romFile->hasHomebrewGameCode();
                
                if (hasHomebrewGameCode || databaseEntry == nullptr) {
                    // Version field  for homebrew roms can't be trusted, so keep it a 1
                    lastGame.romFile.version = 1;

                    homebrewGroups.emplace_back(lastGame);
                }
                else {
                    entryIndexMap[uniqueID] = entryIndex;

                    retailGroups.emplace_back(lastGame);
                    entryIndex++;
                }
            }
            else {
                GameGroup& gameGroup = retailGroups.at(foundRetailEntryIndex);
                gameGroup.addGame(lastGame);
            }

            if (std::find(recentsPaths.begin(), recentsPaths.end(), fullPath) != recentsPaths.end()) {
                recentGroups.emplace_back(lastGame);
            }

            if (std::find(favouritesPaths.begin(), favouritesPaths.end(), fullPath) != favouritesPaths.end()) {
                lastGame.isFavourite = true;

                favouriteGroups.emplace_back(lastGame);
            }
        } while (scanner.next());
    }

    database.releaseEntriesFromMemory();

    // fileIndex is the last 0-based index; the count is one more than that
    cachedFileCount = fileIndex + 1;

    if (!readFromCache) {
        writeCache();
    }

    // Sort retail games alphabetically by display title (case-insensitive)
    std::sort(retailGroups.begin(), retailGroups.end(), [](const GameGroup& a, const GameGroup& b) {
        return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
    });

    // Sort homebrew games alphabetically by display title (case-insensitive)
    std::sort(homebrewGroups.begin(), homebrewGroups.end(), [](const GameGroup& a, const GameGroup& b) {
        return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
    });

    auto orderIn = [](const std::vector<std::string>& paths, const GameGroup& group) {
        auto it = std::find(paths.begin(), paths.end(), group.preferredGame()->romFile.path);
        return it != paths.end() ? (int)std::distance(paths.begin(), it) : INT_MAX;
    };

    // Sort recent games by the order they appear in the INI file
    std::sort(recentGroups.begin(), recentGroups.end(), [&](const GameGroup& a, const GameGroup& b) {
        return orderIn(recentsPaths, a) < orderIn(recentsPaths, b);
    });

    // Sort favourite games by the order they appear in the INI file
    std::sort(favouriteGroups.begin(), favouriteGroups.end(), [&](const GameGroup& a, const GameGroup& b) {
        return orderIn(favouritesPaths, a) < orderIn(favouritesPaths, b);
    });

    // The most recently launched game is the first entry in recentGroups, which
    // the sort above put in INI order. Left null when there are no recents.
    lastLaunchedGame = recentGroups.empty() ? nullptr : recentGroups.front().preferredGame();

    debugf("[GameLibrary] File count %i\n", fileIndex);
    debugf("[GameLibrary] Loaded %i Retail Games\n", retailGroups.size());
    debugf("[GameLibrary] Loaded %i Homebrew Games\n", homebrewGroups.size());
    debugf("[GameLibrary] Loaded %i Recent Games\n", recentGroups.size());
    debugf("[GameLibrary] Loaded %i Favourite Games\n", favouriteGroups.size());
}

void GameLibrary::loadM64Files() {
    DirectoryScanner scanner;

     // .m64 files live in the speedruns/ folder on the SD card
    char m64sPath[256];
    snprintf(m64sPath, sizeof(m64sPath), "%sspeedruns/", path);

    debugf("[GameLibrary] Reading M64 files in %s\n", m64sPath);

    // Only proceed if folder exists
    if (scanner.open(m64sPath)) {
        do {
            // Skip directories
            if (scanner.isDirectory) {
                continue;
            }

            const char* filename = scanner.name;

            // Skip files without a valid extension (e.g. .m64)
            if (!M64File::hasM64Extension(filename)) {
                continue;
            }

            M64File* m64File = new M64File(scanner.fileObject);
            m64File->filename = filename;

            if (m64File->loadHeader()) {
                uint32_t crc = m64File->romCRC;

                // Attach this recording to the first loaded speedrun game whose
                // ROM CRC matches it.
                for (Game* game : speedrunGames) {
                    if (game->romFile.crc1 == crc) {
                        game->m64Files.push_back(m64File);
                        break;
                    }
                }
            }
        } while (scanner.next());
    }

    // debugf("[GameLibrary] Loaded %i M64 Files\n", allM64Files.size());
    debugf("[GameLibrary] Loaded %i Speedrun Games\n", speedrunGames.size());
}

bool GameLibrary::writeRecordedSpeedrun(Game& game, uint32_t inputsAddress, uint32_t sampleCount) {
    // The payload only ever records into the 0x80600000-0x80700000 table, so
    // clamp a count that would run past it rather than trusting it
    sampleCount = std::min(sampleCount, (uint32_t)(MiB(1) / sizeof(uint32_t)));

    if (sampleCount == 0) {
        return false;
    }

    char directoryPath[256];
    snprintf(directoryPath, sizeof(directoryPath), "%sspeedruns", path);

    if (!directory_exists(directoryPath) && directory_create(directoryPath)) {
        debugf("[GameLibrary] Failed to create %s\n", directoryPath);
        return false;
    }

    // First free "<ID> NNN.m64", so earlier recordings are never overwritten
    char filePath[320];
    std::string baseName = game.directoryName();

    for (int index = 1; ; index++) {
        snprintf(filePath, sizeof(filePath), "%s/%s %03d.m64", directoryPath, baseName.c_str(), index);

        if (!file_exists(filePath)) {
            break;
        }
    }

    FILE* file = fopen(filePath, "wb");

    if (file == nullptr) {
        debugf("[GameLibrary] Failed to open %s\n", filePath);
        return false;
    }

    // Mupen64's numeric fields are little-endian, so byteswap them. The ROM CRC
    // is the exception: M64File::loadHeader() compares it as read against
    // ROMFile::crc1, so it's written as-is for the recording to match its game.
    M64File::Header header = {};

    memcpy(header.signature, "M64\x1A", 4);
    header.version         = __builtin_bswap32(3);
    header.viFrames        = __builtin_bswap32(sampleCount);
    header.fps             = (game.romFile.tvType() == BOOT_TV_TYPE_PAL) ? 50 : 60;
    header.controllerCount = 1;
    header.inputSamples    = __builtin_bswap32(sampleCount);
    header.startType       = __builtin_bswap16(2);
    header.controllerFlags = __builtin_bswap32(1);
    header.romCRC32        = game.romFile.crc1;
    header.countryCode     = __builtin_bswap16((uint16_t)game.romFile.regionCode);
    header.extAuthorship   = M64File::WONDERMENU;

    memcpy(header.romName, game.romFile.title, sizeof(game.romFile.title) - 1);

    bool success = (fwrite(&header, sizeof(header), 1, file) == 1);

    // The samples are already in the byte order playback expects (it stages the
    // file's input data straight into the table), so they're copied verbatim
    const uint32_t chunkSize = KiB(16);
    uint8_t* buffer = (uint8_t*)malloc_uncached(chunkSize);

    uint32_t remaining = sampleCount * sizeof(uint32_t);
    uint32_t offset = 0;

    while (success && remaining > 0) {
        uint32_t length = std::min(remaining, chunkSize);

        dma_read(buffer, inputsAddress + offset, length);

        success = (fwrite(buffer, 1, length, file) == length);

        offset += length;
        remaining -= length;
    }

    free_uncached(buffer);

    if (fclose(file) != 0) {
        success = false;
    }

    debugf("[GameLibrary] %s %s (%lu samples)\n", success ? "Wrote" : "Failed to write", filePath, sampleCount);

    return success;
}