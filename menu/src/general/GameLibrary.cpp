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

// The cache records a djb2 hash of each ROM's filename rather than its path,
// so recents and favourites are matched by hashing their own filename the
// same way. Hashed up front: otherwise it would be every path, every entry.
static std::vector<uint32_t> filenameHashes(const std::vector<std::string>& romPaths) {
    std::vector<uint32_t> hashes;
    hashes.reserve(romPaths.size());

    for (const std::string& romPath : romPaths) {
        size_t separator = romPath.rfind('/');

        const char* filename = (separator == std::string::npos)
            ? romPath.c_str()
            : romPath.c_str() + separator + 1;

        hashes.push_back(GameDatabase::djb2((const unsigned char*)filename, strlen(filename)));
    }

    return hashes;
}

// `hashes` is `filenameHashes(paths)`. The hash rules out almost every path for
// the cost of a compare, and the full path then confirms a match, so a ROM of
// the same name in another folder doesn't match too.
static bool containsPath(const std::vector<uint32_t>& hashes, const std::vector<std::string>& paths, uint32_t hash, const char* fullPath) {
    for (size_t i = 0; i < hashes.size(); i++) {
        if (hashes[i] == hash && paths[i] == fullPath) {
            return true;
        }
    }

    return false;
}

// Alphabetical by display title (case-insensitive). mkdatabase ranks database
// titles with the same strcasecmp, so the two orders agree when they're mixed.
static bool compareTitles(const GameGroup& a, const GameGroup& b) {
    int sortIndexA = a.preferredGame()->titleSortIndex();
    int sortIndexB = b.preferredGame()->titleSortIndex();

    if (sortIndexA >= 0 && sortIndexB >= 0) {
        return sortIndexA < sortIndexB;
    }

    return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
}

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

    if (!temporaryCacheHeader.isValid()) {
        debugf("[GameLibrary] Cache file isn't a version %i cache\n", CacheHeader::CURRENT_VERSION);
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

    allocateCache();

    // Don't read past the bounds of the cache array, or the end of the file
    size_t romFileCount = temporaryCacheHeader.romFileCount;
    size_t availableEntries = (fileSize - entriesOffset) / sizeof(ROMCacheResult);
    size_t bytesToRead = std::min({romFileCount, romCache.size(), availableEntries}) * sizeof(ROMCacheResult);

    if (f_read(&cacheFile, romCache.data(), bytesToRead, &bytesRead) != FR_OK || bytesRead != bytesToRead) {
        debugf("[GameLibrary] Failed to read cache file (read %u of %zu bytes)\n", (unsigned)bytesRead, bytesToRead);
        f_close(&cacheFile);
        return false;
    }

    readFromCache = true;

    // The M64 entries follow every ROM entry. A cache cut short before them
    // just has none, so each M64 file is read again.
    size_t m64EntriesOffset = entriesOffset + (romFileCount * sizeof(ROMCacheResult));
    size_t availableM64Entries = (fileSize > m64EntriesOffset) ? (fileSize - m64EntriesOffset) / sizeof(M64CacheResult) : 0;

    m64Cache.resize(std::min((size_t)temporaryCacheHeader.m64FileCount, availableM64Entries));

    bytesToRead = m64Cache.size() * sizeof(M64CacheResult);

    readM64sFromCache = true;

    if (bytesToRead > 0) {
        if (f_lseek(&cacheFile, m64EntriesOffset) != FR_OK
            || f_read(&cacheFile, m64Cache.data(), bytesToRead, &bytesRead) != FR_OK
            || bytesRead != bytesToRead
        ) {
            debugf("[GameLibrary] Failed to read M64 cache entries\n");

            m64Cache.clear();
            readM64sFromCache = false;
        }
    }

    f_close(&cacheFile);

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

    size_t romEntriesToWrite = std::min((size_t)cachedROMFileCount, (size_t)MAX_NUMBER_OF_ROM_FILES);
    size_t m64EntriesToWrite = std::min((size_t)cachedM64FileCount, m64Cache.size());

    CacheHeader header = {};
    header.directoryCount = directoryCount();
    header.romFileCount = romEntriesToWrite;
    header.m64FileCount = m64EntriesToWrite;

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

    size_t romEntriesWritten = fwrite(romCache.data(), sizeof(ROMCacheResult), romEntriesToWrite, cacheFile);
    size_t m64EntriesWritten = fwrite(m64Cache.data(), sizeof(M64CacheResult), m64EntriesToWrite, cacheFile);

    fclose(cacheFile);

    bool didWriteROMEntries = (romEntriesWritten == romEntriesToWrite);
    bool didWriteM64Entries = (m64EntriesWritten == m64EntriesToWrite);

    if (!didWriteROMEntries || !didWriteM64Entries) {
        debugf("[GameLibrary] Failed to write cache file (wrote %zu of %zu entries, %zu of %zu M64 entries)\n", romEntriesWritten, romEntriesToWrite, m64EntriesWritten, m64EntriesToWrite);

        return false;
    }
    else {
        debugf("[GameLibrary] Wrote cache file. Entries: %zu, M64 entries: %zu\n", romEntriesWritten, m64EntriesWritten);

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

    while (recentsPaths.size() < MAX_NUMBER_OF_RECENTS && parser.read(iniEntry)) {
        if (iniEntry.primaryPath.empty()) {
            continue;
        }

        recentsPaths.push_back(iniEntry.primaryPath);
    }

    parser.beginSection("favorite");

    while (favouritesPaths.size() < MAX_NUMBER_OF_FAVOURITES && parser.read(iniEntry)) {
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

    // See `INIParser.h` for file format example
    std::string contents;
    contents.reserve(KiB(8));

    auto writeSection = [&contents](const char* section, const std::vector<GameGroup>& groups) {
        contents += '[';
        contents += section;
        contents += "]\n";

        int index = 0;

        for (const GameGroup& group : groups) {
            const Game* game = group.preferredGame();

            if (game == nullptr) {
                continue;
            }

            std::string prefix = std::to_string(index);

            contents += prefix + "_primary_path=" + game->romFile.path + "\n";
            contents += prefix + "_secondary_path=\n";
            contents += prefix + "_type=1\n";

            index++;
        }
    };

    writeSection("history", recentGroups);
    contents += '\n';
    writeSection("favorite", favouriteGroups);

    // FA_OPEN_ALWAYS rather than truncating on open, so the write starts in the
    // file's existing first cluster instead of freeing its chain and allocating
    // a new one. f_truncate() then only drops what's past the new end.
    FIL file = {};

    if (f_open(&file, strip_fs_prefix(iniPath), FA_WRITE | FA_OPEN_ALWAYS) != FR_OK) {
        debugf("[GameLibrary] Failed to open INI file for writing: %s\n", iniPath);
        return false;
    }

    UINT bytesWritten = 0;

    FRESULT writeResult = f_write(&file, contents.data(), contents.size(), &bytesWritten);
    FRESULT truncateResult = f_truncate(&file);
    FRESULT closeResult = f_close(&file);

    if (writeResult != FR_OK || bytesWritten != contents.size() || truncateResult != FR_OK || closeResult != FR_OK) {
        debugf("[GameLibrary] Failed to write INI file: %s\n", iniPath);
        return false;
    }

    return true;
}

void GameLibrary::addToRecents(GameGroup gameGroup) {
    Game* game = gameGroup.preferredGame();

    recentGroups.erase(
        std::remove(recentGroups.begin(), recentGroups.end(), gameGroup),
        recentGroups.end()
    );

    recentGroups.emplace(recentGroups.begin(), *game);

    if (recentGroups.size() > MAX_NUMBER_OF_RECENTS) {
        recentGroups.erase(recentGroups.begin() + MAX_NUMBER_OF_RECENTS, recentGroups.end());
    }

    saveHistoryAndFavourites();
}

void GameLibrary::toggleFavourite(Game& game) {
    game.isFavourite = !game.isFavourite;

    if (!game.isFavourite) {
        // Each favourite is a group of just its own game.
        auto it = std::find_if(favouriteGroups.begin(), favouriteGroups.end(), [&](GameGroup& group) {
            return group.preferredGame() == &game;
        });

        if (it != favouriteGroups.end()) {
            favouriteGroups.erase(it);
        }
    }
    else {
        favouriteGroups.emplace(favouriteGroups.begin(), game);

        // The oldest favourite drops off the end, so it's no longer one
        while (favouriteGroups.size() > MAX_NUMBER_OF_FAVOURITES) {
            favouriteGroups.back().preferredGame()->isFavourite = false;
            favouriteGroups.pop_back();
        }
    }

    saveHistoryAndFavourites();
}

void GameLibrary::toggleRetailGroupings() {
    groupRetailGames = !groupRetailGames;

    retailGroups.clear();

    GameIDMap<int16_t, -1> entryIndexMap;
    // Reset to the -1 "unset" sentinel.
    entryIndexMap.reset();

    int entryIndex = 0;

    for (Game& game : allGames) {
        if (game.hasHomebrewGameCode()) {
            continue;
        }

        ROMFile& romFile = game.romFile;

        char* uniqueID = romFile.uniqueID;
        int foundRetailEntryIndex = entryIndexMap[uniqueID];

        if (!groupRetailGames) {
            foundRetailEntryIndex = -1;
        }

        if (foundRetailEntryIndex == -1) {
            entryIndexMap[uniqueID] = entryIndex;

            retailGroups.emplace_back(game);
            entryIndex++;
        }
        else {
            GameGroup& gameGroup = retailGroups.at(foundRetailEntryIndex);
            gameGroup.addGame(game);
        }
    }

    // Sort retail games alphabetically by display title (case-insensitive)
    std::sort(retailGroups.begin(), retailGroups.end(), compareTitles);
}

std::vector<std::string> GameLibrary::findROMDirectories(int depth) {
    std::vector<std::string> romDirectoryPaths;

    if (path == nullptr) {
        return romDirectoryPaths;
    }

    findROMDirectories("", std::max(depth, 0), romDirectoryPaths);

    debugf("[GameLibrary] Found %i ROM folders in %s\n", romDirectoryPaths.size(), path);

    return romDirectoryPaths;
}

void GameLibrary::findROMDirectories(const std::string& relativePath, int depth, std::vector<std::string>& directoryPaths) {
    // The cache stores each directory in a fixed size field, so a longer one
    // couldn't be read back
    if (relativePath.size() >= MAX_DIRECTORY_LENGTH) {
        debugf("[GameLibrary] Skipping %s, its path is too long\n", relativePath.c_str());
        return;
    }

    char directoryPath[512];

    if (relativePath.empty()) {
        snprintf(directoryPath, sizeof(directoryPath), "%s", path);
    }
    else {
        snprintf(directoryPath, sizeof(directoryPath), "%s%s/", path, relativePath.c_str());
    }

    dir_t entry;

    if (dir_findfirst(directoryPath, &entry) != 0) {
        return;
    }

    bool hasROMs = false;
    std::vector<std::string> subdirectoryNames;

    // Finished before searching any subdirectory, so only one listing is open
    // at a time
    do {
        if (entry.d_type == DT_DIR) {
            if (depth > 0 && entry.d_name[0] != '.') {
                subdirectoryNames.push_back(entry.d_name);
            }
        }
        else if (!hasROMs && ROMFile::hasROMExtension(entry.d_name)) {
            hasROMs = true;
        }
    } while (dir_findnext(directoryPath, &entry) == 0);

    if (hasROMs) {
        directoryPaths.push_back(relativePath);
    }

    std::sort(subdirectoryNames.begin(), subdirectoryNames.end(), [](const std::string& a, const std::string& b) {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    });

    for (const std::string& subdirectoryName : subdirectoryNames) {
        std::string subdirectoryPath = relativePath.empty()
            ? subdirectoryName
            : relativePath + "/" + subdirectoryName;

        findROMDirectories(subdirectoryPath, depth - 1, directoryPaths);
    }
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
            return a->data.smallLabelTileIndex < b->data.smallLabelTileIndex;
        }
    );

    database.warmClusterOffsets(entries);
}

void GameLibrary::loadGames() {
    createDirectories();
    
    retailGroups.clear();

    // On the stack, as it's only needed here. 16 bits is plenty for an index
    // into retailGroups, and halves it to 2.5 KB.
    GameIDMap<int16_t, -1> entryIndexMap;
    entryIndexMap.reset();

    // Already allocated if loadCache() found a cache file
    allocateCache();

    DirectoryScanner scanner;

    int fileIndex = -1;
    int entryIndex = 0;

    std::vector<uint32_t> recentsHashes = filenameHashes(recentsPaths);
    std::vector<uint32_t> favouritesHashes = filenameHashes(favouritesPaths);

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

        size_t scanPathLength = strlen(scanPath);

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
            if (fileIndex + 1 >= MAX_NUMBER_OF_ROM_FILES) {
                debugf("[GameLibrary] Reached the %i file limit\n", MAX_NUMBER_OF_ROM_FILES);
                break;
            }

            // Build full path. Both lengths are already known, so it's two
            // copies rather than snprintf parsing a format string per ROM.
            // Sized so scanPath (up to 511) + the name (up to 255) and its
            // terminator always fit.
            char fullPath[768];
            memcpy(fullPath, scanPath, scanPathLength);
            memcpy(fullPath + scanPathLength, filename, filenameLength + 1);

            ROMFile& romFile = *new ROMFile(fullPath, fileIndex);

            // Lets loadHeader() below open the file with no directory lookup
            romFile.fileObject = scanner.fileObject;

            GameDatabase::Entry* databaseEntry = nullptr;

            uint32_t hash = GameDatabase::djb2((const unsigned char*)filename, filenameLength);
            uint32_t fileSize = scanner.size;

            // Set the ROM size here so it is populated on the cache-hit path too;
            // loadHeader() (cache-miss only) would otherwise be the sole place it is
            // set, leaving romFile.size uninitialised for cached games.
            romFile.size = fileSize;

            fileIndex++;

            ROMCacheResult cacheResult = romCache[fileIndex];

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

                romFile.version = cacheResult.version;
                romFile.crc1 = cacheResult.crc1;
            }
            else {
                romCache[fileIndex] = ROMCacheResult(hash, fileSize, -1, 0, 0);
                
                if (romFile.loadHeader()) {
                    int16_t databaseIndex = database.indexForROMFile(romFile);

                    if (databaseIndex != -1) {
                        databaseEntry = database.readEntryAtIndex(databaseIndex);
                    }

                    romCache[fileIndex].entryIndex = databaseIndex;
                    romCache[fileIndex].version = romFile.version;
                    romCache[fileIndex].crc1 = romFile.crc1;
                }
                else {
                    debugf("[GameLibrary] Failed to load ROM file: %s\n", filename);

                    // Failed to load, clean up
                    delete &romFile;

                    continue;
                }
            }

            if (databaseEntry != nullptr) {
                romFile.uniqueID[0] = databaseEntry->data.uniqueID[0];
                romFile.uniqueID[1] = databaseEntry->data.uniqueID[1];
                romFile.regionCode = databaseEntry->data.regionCode;
            }

            Game& lastGame = allGames.emplace_back(romFile, databaseEntry);

            if (databaseEntry != nullptr) {
                if (databaseEntry->hasSpeedrunDatabase()) {
                    speedrunGames.push_back(&lastGame);
                }
            }

            char* uniqueID = romFile.uniqueID;

            int foundRetailEntryIndex = entryIndexMap[uniqueID];

            if (foundRetailEntryIndex == -1) {
                bool hasHomebrewGameCode = romFile.hasHomebrewGameCode();
                
                if (hasHomebrewGameCode || databaseEntry == nullptr) {
                    // Version field for homebrew roms can't be trusted, so keep it at 1.0
                    lastGame.romFile.version = 0;

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

            if (containsPath(recentsHashes, recentsPaths, hash, fullPath)) {
                recentGroups.emplace_back(lastGame);
            }

            if (containsPath(favouritesHashes, favouritesPaths, hash, fullPath)) {
                lastGame.isFavourite = true;

                favouriteGroups.emplace_back(lastGame);
            }
        } while (scanner.next());
    }

    // fileIndex is the last 0-based index; the count is one more than that
    cachedROMFileCount = fileIndex + 1;

    // Sort retail games alphabetically by display title (case-insensitive)
    std::sort(retailGroups.begin(), retailGroups.end(), compareTitles);

    // Sort homebrew games alphabetically by display title (case-insensitive)
    std::sort(homebrewGroups.begin(), homebrewGroups.end(), compareTitles);

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

    debugf("[GameLibrary] File count %i\n", cachedROMFileCount);
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

    int m64FileIndex = -1;

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

            uint32_t hash = GameDatabase::djb2((const unsigned char*)filename, scanner.nameLength);
            uint32_t fileSize = scanner.size;

            m64FileIndex++;

            // A file past the end of the cache gets a blank entry, which can't
            // match it
            if (m64FileIndex >= (int)m64Cache.size()) {
                m64Cache.resize(m64FileIndex + 1);
            }

            M64CacheResult cacheResult = m64Cache[m64FileIndex];

            if (readM64sFromCache) {
                if (cacheResult.hash != hash || cacheResult.fileSize != fileSize) {
                    readM64sFromCache = false;

                    debugf("[GameLibrary] Hash mismatch for M64 file: %s\n", filename);
                    debugf("[GameLibrary] Abandoning M64 cache.\n");
                }
            }

            bool isLoaded;

            if (readM64sFromCache) {
                isLoaded = cacheResult.isLoaded;

                if (isLoaded) {
                    m64File->loadCachedHeader(cacheResult.romCRC, cacheResult.crc, cacheResult.authoringProgram, cacheResult.controllerCount);
                }
            }
            else {
                isLoaded = m64File->loadHeader();

                M64CacheResult& newCacheResult = m64Cache[m64FileIndex];

                newCacheResult = M64CacheResult();
                newCacheResult.hash = hash;
                newCacheResult.fileSize = fileSize;
                newCacheResult.isLoaded = isLoaded;

                if (isLoaded) {
                    newCacheResult.romCRC = m64File->romCRC;
                    newCacheResult.crc = m64File->crc;
                    newCacheResult.authoringProgram = m64File->authoringProgram;
                    newCacheResult.controllerCount = m64File->controllerCount;
                }
            }

            if (!isLoaded) {
                debugf("[GameLibrary] Failed to load M64 file: %s\n", filename);

                // Failed to load, clean up
                delete m64File;

                continue;
            }

            uint32_t crc = m64File->romCRC;

            // Attach this recording to the first loaded speedrun game whose
            // ROM CRC matches it.
            for (Game* game : speedrunGames) {
                if (game->romFile.crc1 == crc) {
                    game->m64Files.push_back(m64File);
                    break;
                }
            }
        } while (scanner.next());
    }

    // m64FileIndex is the last 0-based index; the count is one more than that
    cachedM64FileCount = m64FileIndex + 1;

    if ((!readFromCache || !readM64sFromCache) && database.isOpen()) {
        writeCache();
    }

    // Swapped with empty vectors, as clear() alone keeps the allocation
    std::vector<ROMCacheResult>().swap(romCache);
    std::vector<M64CacheResult>().swap(m64Cache);

    // debugf("[GameLibrary] Loaded %i M64 Files\n", allM64Files.size());
    debugf("[GameLibrary] Loaded %i Speedrun Games\n", speedrunGames.size());
}

bool GameLibrary::writeRecordedSpeedrun(Game& game, uint32_t inputsAddress, uint32_t sampleCount, uint32_t controllerCount) {
    // Each sample is one input word per controller. An M64 holds 1 to 4, and
    // the payload clamps to the same range, so this only guards a bad readback.
    controllerCount = std::clamp(controllerCount, (uint32_t)1, (uint32_t)4);

    // The payload only ever records into the M64_INPUTS_MAX_SIZE table, so
    // clamp a count that would run past it rather than trusting it
    sampleCount = std::min(sampleCount, (uint32_t)(M64_INPUTS_MAX_SIZE / (sizeof(uint32_t) * controllerCount)));

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
    header.controllerCount = (uint8_t)controllerCount;
    header.inputSamples    = __builtin_bswap32(sampleCount);
    header.startType       = __builtin_bswap16(2);
    // Bits 0-3: controllers 1-4 present. The payload records ports in order
    // from the first, so they're the lowest bits.
    header.controllerFlags = __builtin_bswap32((1u << controllerCount) - 1);
    header.romCRC32        = game.romFile.crc1;
    header.countryCode     = __builtin_bswap16((uint16_t)game.romFile.regionCode);
    header.extAuthorship   = M64File::WONDERMENU;

    memcpy(header.romName, game.romFile.title, sizeof(game.romFile.title) - 1);

    bool success = (fwrite(&header, sizeof(header), 1, file) == 1);

    // The samples are already in the byte order playback expects (it stages the
    // file's input data straight into the table), so they're copied verbatim
    const uint32_t chunkSize = KiB(16);
    uint8_t* buffer = (uint8_t*)malloc_uncached(chunkSize);

    uint32_t remaining = sampleCount * controllerCount * sizeof(uint32_t);
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

    debugf("[GameLibrary] %s %s (%lu samples, %lu controllers)\n", success ? "Wrote" : "Failed to write", filePath, sampleCount, controllerCount);

    return success;
}