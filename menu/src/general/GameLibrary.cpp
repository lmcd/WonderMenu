/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <algorithm>
#include <climits>
#include <cstring>
#include <strings.h>

#include "DirectoryScanner.h"
#include "GameLibrary.h"

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

bool GameLibrary::loadCache() {
    romDirectoryPaths.clear();

    #ifndef RELEASE_BUILD
    readFromCache = false;
    return false;
    #endif

    char cachePath[512];
    snprintf(cachePath, sizeof(cachePath), "sd:/cache.bin");

    FILE* cacheFile = fopen(cachePath, "rb");

    if (!cacheFile) {
        debugf("[GameLibrary] Failed to open cache file: %s\n", cachePath);
        readFromCache = false;
        return false;
    }

    struct stat st;
    if (fstat(fileno(cacheFile), &st)) {
        debugf("[GameLibrary] Failed to stat cache file: %s\n", cachePath);
        readFromCache = false;
        fclose(cacheFile);
        return false;
    }

    debugf("[GameLibrary] Found cache file. Size: %li\n", st.st_size);

    if (fread(&temporaryCacheHeader, 1, sizeof(CacheHeader), cacheFile) != sizeof(CacheHeader)) {
        debugf("[GameLibrary] Failed to read cache header\n");
        readFromCache = false;
        fclose(cacheFile);
        return false;
    }

    int directoryCount = temporaryCacheHeader.directoryCount;

    if (directoryCount > 0) {
        std::vector<char> directoryBuffer(directoryCount * MAX_DIRECTORY_LENGTH);

        if (fread(directoryBuffer.data(), 1, directoryBuffer.size(), cacheFile) != directoryBuffer.size()) {
            debugf("[GameLibrary] Failed to read %i cache directories\n", directoryCount);

            readFromCache = false;
            fclose(cacheFile);
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

    if ((size_t)st.st_size < entriesOffset) {
        debugf("[GameLibrary] Cache file is shorter than its header claims\n");

        romDirectoryPaths.clear();
        readFromCache = false;
        fclose(cacheFile);
        return false;
    }

    // Don't read past the bounds of the cache array
    size_t bytesToRead = std::min((size_t)st.st_size - entriesOffset, sizeof(cache));

    size_t bytesRead = fread(cache, 1, bytesToRead, cacheFile);
    if (bytesRead != bytesToRead) {
        debugf("[GameLibrary] Failed to read cache file (read %zu of %zu bytes)\n", bytesRead, bytesToRead);
        fclose(cacheFile);
        return false;
    }

    fclose(cacheFile);

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

    // Reserve the header slot with a blank (zeroed) header; the real values are
    // filled in later by writeCacheHeader(). The entries follow it.
    CacheHeader header = {};
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

bool GameLibrary::writeCacheHeader() {
    if (lastLaunchedGame == nullptr) {
        debugf("[GameLibrary] No last launched game; cannot write cache header\n");
        return false;
    }

    ROMFile& romFile = lastLaunchedGame->romFile;

    CacheHeader header = {};
    header.lastUniqueID[0] = romFile.uniqueID[0];
    header.lastUniqueID[1] = romFile.uniqueID[1];
    header.lastRegionCode = romFile.regionCode;
    header.directoryCount = directoryCount();

    char cachePath[512];
    snprintf(cachePath, sizeof(cachePath), "sd:/cache.bin");

    // Update the header in place so the cache entries that follow are preserved.
    FILE* cacheFile = fopen(cachePath, "r+b");
    if (!cacheFile) {
        debugf("[GameLibrary] Failed to open cache file for header write: %s\n", cachePath);
        return false;
    }

    fseek(cacheFile, 0, SEEK_SET);
    size_t bytesWritten = fwrite(&header, 1, sizeof(CacheHeader), cacheFile);

    fclose(cacheFile);

    if (bytesWritten != sizeof(CacheHeader)) {
        debugf("[GameLibrary] Failed to write cache header (wrote %zu of %zu bytes)\n", bytesWritten, sizeof(CacheHeader));
        return false;
    }

    debugf("[GameLibrary] Wrote cache header. Game: %c%c\n", header.lastUniqueID[0], header.lastUniqueID[1]);
    return true;
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

void GameLibrary::loadGames() {
    retailGroups.clear();

    entryIndexMap.reset();

    DirectoryScanner scanner;

    int fileIndex = -1;
    int entryIndex = 0;

    std::vector<Game*> speedrunGames;

    // Move database entires from cart space into memory for quicker retrieval.
    // NOTE: this suprisingly only saves about 22ms of load time for a library
    // of 900 games. Hmmm...
    // database.loadEntriesIntoMemory();

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

            // Skip files without a valid extension (e.g. .z64)
            if (!ROMFile::hasROMExtension(filename)) {
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

            ROMFile* romFile = new ROMFile(fullPath);

            #ifdef N64
            // Lets loadHeader() below open the file with no directory lookup
            romFile->fileObject = scanner.fileObject;
            #endif

            GameDatabase::Entry* databaseEntry = nullptr;

            uint32_t hash = GameDatabase::djb2((const unsigned char*)filename, strlen(filename));
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
                romFile->uniqueID[0] = databaseEntry->uniqueID[0];
                romFile->uniqueID[1] = databaseEntry->uniqueID[1];
                romFile->regionCode = databaseEntry->regionCode;
            }

            Game& lastGame = allGames.emplace_back(*romFile, databaseEntry);

            if (databaseEntry != nullptr) {
                if (databaseEntry->supportsSpeedruns) {
                    speedrunGames.push_back(&lastGame);
                }
            }

            char* uniqueID = romFile->uniqueID;

            int foundRetailEntryIndex = entryIndexMap[uniqueID];

            bool hasHomebrewGameCode = romFile->hasHomebrewGameCode();

            if (hasHomebrewGameCode) {
                foundRetailEntryIndex = -1;
            }

            if (foundRetailEntryIndex == -1) {
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

    // database.releaseEntriesFromMemory();

    // fileIndex is the last 0-based index; the count is one more than that
    cachedFileCount = fileIndex + 1;

    // Restore the last launched game recorded in the cache header, matching on
    // the unique ID and region code. Left null if no game matches.
    lastLaunchedGame = nullptr;
    for (Game& game : allGames) {
        if (game.romFile.uniqueID[0] == temporaryCacheHeader.lastUniqueID[0] &&
            game.romFile.uniqueID[1] == temporaryCacheHeader.lastUniqueID[1] &&
            game.romFile.regionCode == temporaryCacheHeader.lastRegionCode) {
            lastLaunchedGame = &game;

            debugf("[GameLibrary] Found last launched game: %s\n", game.title());
            break;
        }
    }

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

    // .m64 files live in the speedruns/ folder on the SD card
    char m64sPath[512];
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

            // Sized so m64sPath (up to 511) + d_name (up to 255) can't truncate,
            // keeping -Werror=format-truncation happy.
            char m64Path[768];
            snprintf(m64Path, sizeof(m64Path), "%s%s", m64sPath, filename);

            M64File m64File = M64File(m64Path);

            if (m64File.loadHeader()) {
                uint32_t crc = m64File.romCRC32;

                // Attach this recording to the first loaded speedrun game whose
                // ROM CRC matches it.
                for (Game* game : speedrunGames) {
                    if (game->romFile.crc1 == crc) {
                        allM64Files.push_back(std::move(m64File));
                        game->m64Files.push_back(&allM64Files.back());
                        break;
                    }
                }
            }
        } while (scanner.next());
    }

    debugf("[GameLibrary] File count %i\n", fileIndex);
    debugf("[GameLibrary] Loaded %i Retail Games\n", retailGroups.size());
    debugf("[GameLibrary] Loaded %i Homebrew Games\n", homebrewGroups.size());
    debugf("[GameLibrary] Loaded %i Recent Games\n", recentGroups.size());
    debugf("[GameLibrary] Loaded %i Favourite Games\n", favouriteGroups.size());
}
