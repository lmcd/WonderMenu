/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <chrono>

#include "GameDatabase.h"
#include "utils/fs.h"

#define KiB(x)         ((x) * 1024)
#define MiB(x)         ((x) * 1024 * 1024)

// DMA reads from ROM only work on hardware/flashcart; enable for release builds only
#ifdef RELEASE_BUILD
#define USE_DMA        true
#else
#define USE_DMA        false
#endif

#ifdef N64
#define ROM_ADDRESS_HEADER   (0x10000000 + rom_scratch_offset())
#define ROM_ADDRESS_TILES_A  (ROM_ADDRESS_HEADER  + KiB(256))
#define ROM_ADDRESS_TILES_B  (ROM_ADDRESS_TILES_A + DB_L_LABEL_SIZE)
// Scratch cart SDRAM used to stage database reads before they're DMA'd into RDRAM.
// This has to sit past the end of our own ROM image: the DragonFS every menu asset
// is loaded from (fonts, sprites, models) lives in that same SDRAM, so staging over
// it silently corrupts the filesystem and `rom:/` opens start failing with ENOENT.
// The ROM size is patched into header offset 0x18 by the `rom-with-size` make
// target; round it up to the next MiB, with a 2 MiB floor so an unpatched or
// implausible header can't drop the scratch back inside the ROM.
static uint32_t rom_scratch_offset(void) {
    static uint32_t offset = 0;

    if (offset == 0) {
        uint32_t romSize = io_read(0x10000000 + 0x18);

        offset = (romSize > 0 && romSize <= MiB(16))
            ? ((romSize + MiB(1) - 1) & ~(uint32_t)(MiB(1) - 1))
            : MiB(2);

        if (offset < MiB(2)) {
            offset = MiB(2);
        }
    }

    return offset;
}
#endif

static unsigned char headerBuffer[DB_ENTRIES_START_OFFSET];

uint32_t GameDatabase::djb2(const unsigned char* data, size_t length) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

// GameDatabase::Entry implementation
GameDatabase::Entry::Entry() : uniqueID("0") {
    memset(title, 0, sizeof(title));
    memset(developer, 0, sizeof(developer));
    memset(uniqueID, 0, sizeof(uniqueID));

    cheatByteOffset = 0;
}

// GameDatabase implementation
GameDatabase::GameDatabase() : databaseFile(nullptr) {
    titleLookupMap.reserve(10);
}

GameDatabase::~GameDatabase() {
    if (databaseFile) {
        fclose(databaseFile);
        databaseFile = nullptr;
    }
}

int GameDatabase::getLabelTileOffset(int tileIndex) {
    return labelsStartOffset + (tileIndex * DB_LABEL_TILE_SIZE) + (DB_LABEL_PADDING_PER_CHUNK * (tileIndex / DB_LABEL_TILES_PER_CHUNK));
}

GameDatabase::Entry* GameDatabase::readEntryAtIndex(uint16_t index) {
    if (!databaseFile) {
        return nullptr;
    }

    size_t size = DB_ENTRY_SIZE;
    uint32_t offset = entriesStartOffset + (size * index);

    Entry* entry = new Entry();
    entry->entryIndex = index;

    if (USE_DMA) {
        #ifdef N64
        data_cache_hit_writeback_invalidate(entry, size);
        dma_read_raw_async(entry, ROM_ADDRESS_HEADER + offset, size);
        dma_wait();
        #endif
    }
    else {
        if (fseek(databaseFile, offset, SEEK_SET) != 0) {
            delete entry;
            return nullptr;
        }

        if (fread(entry, size, 1, databaseFile) != 1) {
            delete entry;
            return nullptr;
        }
    }

    return entry;
}

RegionCode GameDatabase::readRegionCodeAtIndex(uint16_t index) {
    // The regionCode byte is stored after title, developer, categoryCode and uniqueID
    uint32_t offset = entriesStartOffset + (DB_ENTRY_SIZE * index)
        + sizeof(Entry::title)
        + sizeof(Entry::developer)
        + sizeof(Entry::categoryCode)
        + sizeof(Entry::uniqueID);

    if (fseek(databaseFile, offset, SEEK_SET) != 0) {
        return RegionCode::UNKNOWN;
    }

    RegionCode regionCode = RegionCode::UNKNOWN;
    if (fread(&regionCode, sizeof(regionCode), 1, databaseFile) != 1) {
        return RegionCode::UNKNOWN;
    }

    return regionCode;
}

int16_t GameDatabase::indexForTitle(char title[21]) {
    std::string romTitle = std::string(title, strnlen(title, 20));
    uint32_t hash = djb2((const unsigned char*)romTitle.c_str(), romTitle.length());

    //debugf("Calculating hash %s;  %i\n", romTitle.c_str(), romTitle.length());
    //debugf("Looking up homebrew: %s %08lX\n", romTitle.c_str(), hash);

    auto it = titleLookupMap.find(hash);
    if (it == titleLookupMap.end()) {
        return -1;
    }

    return it->second;
}

int16_t GameDatabase::indexForUniqueID(char uniqueID[2], RegionCode regionCode, uint8_t version) {
    DatabaseLookupEntry lookupEntry = gameIDMap[uniqueID];

    uint16_t index = lookupEntry.indexStart();
    uint16_t variants = lookupEntry.numberOfVariants();

    if (variants != 0) {
        // The mask knows which entry this version/region pair is without touching
        // the database at all, so try it first.
        int maskIndex = lookupEntry.entryIndexFor(version, regionCode);

        if (maskIndex >= 0) {
            return (int16_t)maskIndex;
        }

        // No bit for it: either the variant genuinely isn't in the database, or
        // it's one the mask has no room for (a region outside its five, or a v3
        // that isn't Japan). Those entries are still written after the ones the
        // mask covers, so fall back to scanning the run for a matching region.

        // TODO: this is costing us 320ms
        for (uint16_t v = 0; v < variants; v++) {
            int variantIndex = v;

            if (readRegionCodeAtIndex(index + variantIndex) == regionCode) {
                return index + variantIndex;
            }
        }
    }

    return -1;
}

GameDatabase::Entry* GameDatabase::entryForTitle(char title[21]) {
    int16_t index = indexForTitle(title);

    if (index == -1) {
        return nullptr;
    }

    return readEntryAtIndex(index);
}

GameDatabase::Entry* GameDatabase::entryForUniqueID(char uniqueID[2], RegionCode regionCode, uint8_t version) {
    // Signed: indexForUniqueID returns -1 for "not found", which as a uint16_t
    // would compare as 65535 and send that straight to readEntryAtIndex.
    int16_t index = indexForUniqueID(uniqueID, regionCode, version);

    if (index == -1) {
        return nullptr;
    }

    return readEntryAtIndex(index);
}

GameDatabase::Entry* GameDatabase::entryForROMFile(ROMFile* romFile) {
    if (!databaseFile) {
        return nullptr;
    }
    
    if (romFile->hasHomebrewGameCode()) {
        return entryForTitle(romFile->title);
    }
    else {
        return entryForUniqueID(romFile->uniqueID, romFile->regionCode, romFile->version);
    }
}

void GameDatabase::warmLabelClusterOffset(Entry* entry) {
    if (!databaseFile) {
        return;
    }

    #ifndef RELEASE_BUILD
    return;
    #endif

    #ifdef N64
    uint16_t tileIndex = entry->labelTileOffset;
    int tileOffset = getLabelTileOffset(tileIndex);

    if (USE_DMA) {
        if (entry->labelTileCluster == 0) {
            f_lseek(&fil, tileOffset);
            entry->labelTileCluster = fil.clust;
        }
    }
    #endif
}

#ifdef N64
bool GameDatabase::loadCartLabelChunk(Entry* entry, bool highRes, char* buffer, int chunkOffset, int chunkCount) {
    if (!databaseFile) {
        debugf("no entry!\n");
        return false;
    }

    if (!entry) {
        placeholderEntry.labelTileOffset = 0;
        placeholderEntry.highResLabelTileOffset = 4;

        entry = &placeholderEntry;
    }
    else {
        if (highRes && entry->highResLabelTileOffset == 0) {
            return false;
        }
    }

    uint16_t tileIndex = highRes ? entry->highResLabelTileOffset : entry->labelTileOffset;
    
    tileIndex += chunkOffset;

    size_t size = DB_LABEL_TILE_SIZE * chunkCount;

    int tileOffset = getLabelTileOffset(tileIndex);

    int bufferOffset = DB_LABEL_TILE_SIZE * chunkOffset;

    if (USE_DMA) {
        if (entry->labelTileCluster != 0 && !highRes) {
            fil.clust = entry->labelTileCluster;
            fil.fptr = tileOffset;
        }
        else {
            f_lseek(&fil, tileOffset);

            if (!highRes) {
                entry->labelTileCluster = fil.clust;
            }
        }

        UINT bytesRead = 0;

        size_t chunk = size;
        chunk += (DB_LABEL_PADDING_PER_CHUNK * (chunkCount / DB_LABEL_TILES_PER_CHUNK));

        f_read(&fil, (void *) (useBSpace ? ROM_ADDRESS_TILES_B : ROM_ADDRESS_TILES_A), chunk, &bytesRead);
        data_cache_hit_writeback_invalidate(buffer + bufferOffset, size);
        dma_read_raw_async(buffer + bufferOffset, (useBSpace ? ROM_ADDRESS_TILES_B : ROM_ADDRESS_TILES_A), size);
        // dma_wait();

        useBSpace = !useBSpace;
    }
    else {
        fseek(databaseFile, tileOffset, SEEK_SET);

        if (fread(buffer + bufferOffset, size, 1, databaseFile) != 1) {
            return false;
        }

        data_cache_hit_writeback_invalidate(buffer + bufferOffset, size);
    }

    #ifdef N64
    // data_cache_hit_writeback(buffer + bufferOffset, size);
    #endif

    return true;
}
#endif

CheatDatabase* GameDatabase::loadCheatDatabase(Entry* entry) {
    if (!databaseFile) {
        return nullptr;
    }

    if (!entry || entry->cheatByteOffset == 0xFFFFFFFF) {
        debugf("[GameDatabase] No cheats for this entry\n");
        return nullptr;
    }

    long offset = cheatsStartOffset + (long)entry->cheatByteOffset;

    debugf("[GameDatabase] Reading cheats at local offset: %li\n", (long)entry->cheatByteOffset);
    debugf("[GameDatabase] Global offset: %li\n", offset);

    if (fseek(databaseFile, offset, SEEK_SET) != 0) {
        debugf("[GameDatabase] Failed to seek to offset %li\n", cheatsStartOffset);
        return nullptr;
    }

    uint16_t counts[4] = {};

    // TODO: Read straight into database header instead
    if (fread(counts, sizeof(uint16_t), 4, databaseFile) != 4) {
        debugf("[GameDatabase] Failed to read cheat counts\n");
        return nullptr;
    }

    uint16_t groupCount = counts[0];
    uint16_t cheatCount = counts[1];
    uint16_t codeCount  = counts[2];
    uint16_t wildcardOptionsCount = counts[3];

    CheatDatabase* cheatDatabase = new CheatDatabase();

    cheatDatabase->groups.resize(groupCount);
    cheatDatabase->cheats.resize(cheatCount);
    cheatDatabase->codes.resize(codeCount);
    cheatDatabase->wildcardOptions.resize(wildcardOptionsCount);

    bool didReadAll =
        fread(cheatDatabase->groups.data(),          sizeof(CheatGroup),          groupCount,           databaseFile) == groupCount
     && fread(cheatDatabase->cheats.data(),          sizeof(Cheat),               cheatCount,           databaseFile) == cheatCount
     && fread(cheatDatabase->codes.data(),           sizeof(CheatCode),           codeCount,            databaseFile) == codeCount
     && fread(cheatDatabase->wildcardOptions.data(), sizeof(CheatWildcardOption), wildcardOptionsCount, databaseFile) == wildcardOptionsCount
    ;

    if (!didReadAll) {
        debugf("[GameDatabase] Failed to read cheat database\n");

        delete cheatDatabase;

        return nullptr;
    }

    return cheatDatabase;
}

#ifdef N64
bool GameDatabase::load(const char* filename) {
    databaseFile = fopen(filename, "rb");

    if (!databaseFile) {
        debugf("Failed to open database file: %s\n", filename);
        return false;
    }

    if (USE_DMA) {
        #ifdef N64
        f_open(&fil, strip_fs_prefix((char*)filename), FA_READ);

        UINT bytesRead = 0;

        size_t chunk = KiB(106);
        f_read(&fil, (void *) (ROM_ADDRESS_HEADER), chunk, &bytesRead);

        data_cache_hit_writeback_invalidate(headerBuffer, sizeof(headerBuffer));
        dma_read_raw_async(headerBuffer, ROM_ADDRESS_HEADER, sizeof(headerBuffer));
        dma_wait();
        #endif
    }
    else {
        if (fread(headerBuffer, sizeof(headerBuffer), 1, databaseFile) != 1) {
            debugf("Failed to read database header\n");
            fclose(databaseFile);
            databaseFile = nullptr;
            return false;
        }
    }

    int cursor = 0;

    // READ HEADER
    // 4 bytes
    char header[4];
    
    memcpy(&header, &headerBuffer[cursor], 4);
    cursor += 4;

    if (header[0] != 'N' || header[1] != 'U' || header[2] != 'F' || header[3] != 'M') {
        debugf("Invalid database header\n");
        fclose(databaseFile);
        databaseFile = nullptr;
        return false;
    }

    // READ ENTRY COUNT
    // 2 bytes
    uint16_t entryCount = 0;

    memcpy(&entryCount, &headerBuffer[cursor], 2);
    cursor += sizeof(entryCount);

    // READ MATCHED BY TITLE COUNT
    // 2 bytes
    uint16_t matchedByTitleCount = 0;

    memcpy(&matchedByTitleCount, &headerBuffer[cursor], 2);
    cursor += sizeof(matchedByTitleCount);

    // READ TILE COUNT
    // 2 bytes
    uint16_t tileCount = 0;

    memcpy(&tileCount, &headerBuffer[cursor], 2);
    cursor += sizeof(tileCount);
    
    // READ GAME ID MAP
    // 5184 bytes
    memcpy(&gameIDMap, &headerBuffer[cursor], sizeof(gameIDMap));
    cursor += sizeof(gameIDMap);

    // Read title lookup entries (djb2 hash + index)
    for (uint16_t i = 0; i < matchedByTitleCount; i++) {
        uint32_t hash = 0;
        uint16_t index = 0;

        memcpy(&hash, &headerBuffer[cursor], sizeof(hash));
        cursor += sizeof(hash);

        memcpy(&index, &headerBuffer[cursor], sizeof(index));
        cursor += sizeof(index);

        titleLookupMap.insert({hash, index});
    }

    entriesStartOffset = DB_ENTRIES_START_OFFSET;

    labelsStartOffset = entriesStartOffset + (entryCount * DB_ENTRY_SIZE);

    long labelsPadding = (512 - (labelsStartOffset % 512)) % 512;
    labelsStartOffset += labelsPadding;

    // The tiles block has DB_LABEL_PADDING_PER_CHUNK bytes of padding after every
    // DB_LABEL_TILES_PER_CHUNK tiles, so the cheats section starts after the padded
    // end of the tiles block. getLabelTileOffset() applies the same padding math.
    cheatsStartOffset = getLabelTileOffset(tileCount);

    debugf("[GameDatabase] Database Entries:     %d\n", entryCount);
    debugf("[GameDatabase] Entries Start Offset: %li\n", entriesStartOffset);
    debugf("[GameDatabase] Pre-labels Padding:   %li\n", labelsPadding);
    debugf("[GameDatabase] Labels Start Offset:  %li\n", labelsStartOffset);
    debugf("[GameDatabase] Cheats Start Offset:  %li\n", cheatsStartOffset);

    return true;
}
#endif

void GameDatabase::close() {
    fclose(databaseFile);
    databaseFile = NULL;
}
