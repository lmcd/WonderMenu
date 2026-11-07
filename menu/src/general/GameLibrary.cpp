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

    allocateCache();

    // Don't read past the bounds of the cache array
    size_t bytesToRead = std::min(fileSize - entriesOffset, cache.size() * sizeof(CacheResult));

    if (f_read(&cacheFile, cache.data(), bytesToRead, &bytesRead) != FR_OK || bytesRead != bytesToRead) {
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

    size_t entriesWritten = fwrite(cache.data(), sizeof(CacheResult), entriesToWrite, cacheFile);

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
    std::sort(retailGroups.begin(), retailGroups.end(), [](const GameGroup& a, const GameGroup& b) {
        return strcasecmp(a.preferredGame()->title(), b.preferredGame()->title()) < 0;
    });
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
            if (fileIndex + 1 >= MAX_NUMBER_OF_FILES) {
                debugf("[GameLibrary] Reached the %i file limit\n", MAX_NUMBER_OF_FILES);
                break;
            }

            // Build full path. Both lengths are already known, so it's two
            // copies rather than snprintf parsing a format string per ROM.
            // Sized so scanPath (up to 511) + the name (up to 255) and its
            // terminator always fit.
            char fullPath[768];
            memcpy(fullPath, scanPath, scanPathLength);
            memcpy(fullPath + scanPathLength, filename, filenameLength + 1);

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
            }
            else {
                cache[fileIndex] = CacheResult(hash, fileSize, 0, 0);
                
                if (romFile->loadHeader()) {
                    databaseEntry = database.entryForROMFile(romFile);

                    cache[fileIndex].entryIndex = (databaseEntry == nullptr) ? -1 : databaseEntry->entryIndex;
                    cache[fileIndex].version = romFile->version;
                    cache[fileIndex].crc1 = romFile->crc1;
                } else {
                    debugf("[GameLibrary] Failed to load ROM file: %s\n", filename);

                    // Failed to load, clean up
                    delete romFile;

                    cache[fileIndex].entryIndex = -1;
                    cache[fileIndex].version = 0;
                    cache[fileIndex].crc1 = 0;

                    continue;
                }
            }

            if (databaseEntry != nullptr) {
                romFile->uniqueID[0] = databaseEntry->data.uniqueID[0];
                romFile->uniqueID[1] = databaseEntry->data.uniqueID[1];
                romFile->regionCode = databaseEntry->data.regionCode;
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
    cachedFileCount = fileIndex + 1;

    if (!readFromCache && database.isOpen()) {
        writeCache();
    }

    // Swapped with an empty vector, as clear() alone keeps the allocation
    std::vector<CacheResult>().swap(cache);

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

bool GameLibrary::writeRecordedSpeedrun(Game& game, uint32_t inputsAddress, uint32_t sampleCount, uint32_t controllerCount) {
    // Each sample is one input word per controller. An M64 holds 1 to 4, and
    // the payload clamps to the same range, so this only guards a bad readback.
    controllerCount = std::clamp(controllerCount, (uint32_t)1, (uint32_t)4);

    // The payload only ever records into the 0x80600000-0x80700000 table, so
    // clamp a count that would run past it rather than trusting it
    sampleCount = std::min(sampleCount, (uint32_t)(MiB(1) / (sizeof(uint32_t) * controllerCount)));

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