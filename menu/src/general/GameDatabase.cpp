/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <chrono>
#include <cstdlib>
#ifdef N64
#include <malloc.h>
#endif

#include "GameDatabase.h"
#include "utils/fs.h"
#include "utils/utils.h"

#ifdef N64
#define ROM_ADDRESS_HEADER   (ROM_ADDRESS + rom_scratch_offset())
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
        uint32_t romSize = io_read(ROM_ADDRESS + 0x18);

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

    // Unrolled by four. The arithmetic is three single-cycle ops per byte, so
    // the loop's own increment and branch were a third of the instructions; the
    // byte offsets fold into the loads instead.
    const unsigned char* end = data + length;
    const unsigned char* blockEnd = data + (length & ~(size_t)3);

    while (data < blockEnd) {
        hash = ((hash << 5) + hash) + data[0];
        hash = ((hash << 5) + hash) + data[1];
        hash = ((hash << 5) + hash) + data[2];
        hash = ((hash << 5) + hash) + data[3];

        data += 4;
    }

    while (data < end) {
        hash = ((hash << 5) + hash) + *data++;
    }

    return hash;
}

// GameDatabase implementation
GameDatabase::GameDatabase() {
    titleLookupMap.reserve(10);
}

GameDatabase::~GameDatabase() {
    close();
}

bool GameDatabase::isDatabaseOpen() {
    #ifndef N64
    return false;
    #else
    return file.obj.fs != nullptr;
    #endif
}

bool GameDatabase::seekTo(long offset) {
    #ifndef N64
    return false;
    #else
    return f_lseek(&file, offset) == FR_OK;
    #endif
}

bool GameDatabase::readBytes(void* buffer, size_t size) {
    #ifndef N64
    return false;
    #else
    UINT bytesRead = 0;

    return f_read(&file, buffer, size, &bytesRead) == FR_OK && bytesRead == size;
    #endif
}

int GameDatabase::getLabelTileOffset(int tileIndex) {
    return labelsStartOffset + (tileIndex * DB_LABEL_TILE_SIZE) + (DB_LABEL_PADDING_PER_CHUNK * (tileIndex / DB_LABEL_TILES_PER_CHUNK));
}

bool GameDatabase::loadEntriesIntoMemory() {
    if (entriesData != nullptr) {
        return true;
    }

    if (!isDatabaseOpen() || entryCount == 0) {
        return false;
    }

    size_t size = (size_t)entryCount * DB_ENTRY_SIZE;

    // PI DMA moves whole 8 byte words, so round the destination up rather than
    // let it write past the end of the block. The overread lands in the padding
    // before the labels section, which is still inside the staged chunk.
    size_t allocationSize = (size + 7) & ~(size_t)7;

    // Aligned for the DMA below; plain malloc everywhere else, where the
    // alignment buys nothing
    #ifdef N64
    unsigned char* data = (unsigned char*)memalign(16, allocationSize);
    #else
    unsigned char* data = (unsigned char*)malloc(allocationSize);
    #endif

    if (data == nullptr) {
        debugf("[GameDatabase] Failed to allocate %u bytes for entries\n", (unsigned)allocationSize);
        return false;
    }

    #ifdef N64
    // `load()` already staged this region in cart SDRAM, so it's one
    // transfer straight out of there
    data_cache_hit_writeback_invalidate(data, allocationSize);
    dma_read_raw_async(data, ROM_ADDRESS_HEADER + entriesStartOffset, allocationSize);
    dma_wait();
    #endif

    entriesData = data;

    debugf("[GameDatabase] Cached %u entries in memory (%u bytes)\n", (unsigned)entryCount, (unsigned)allocationSize);

    return true;
}

void GameDatabase::releaseEntriesFromMemory() {
    if (entriesData == nullptr) {
        return;
    }

    free(entriesData);
    entriesData = nullptr;
}

GameDatabase::Entry* GameDatabase::readEntryAtIndex(uint16_t index) {
    if (!isDatabaseOpen()) {
        return nullptr;
    }

    if (index >= entryCount) {
        return nullptr;
    }

    Entry* entry = new Entry();

    size_t size = DB_ENTRY_SIZE;
    uint32_t offset = entriesStartOffset + (size * index);

    // Fast path: the whole block is resident, so no DMA or filesystem read
    if (entriesData != nullptr) {
        memcpy(entry, entriesData + ((size_t)index * size), size);
    }
    else {
        #ifdef N64
        data_cache_hit_writeback_invalidate(entry, size);
        dma_read_raw_async(entry, ROM_ADDRESS_HEADER + offset, size);
        dma_wait();
        #endif
    }

    entry->entryIndex = index;

    return entry;
}

RegionCode GameDatabase::readRegionCodeAtIndex(uint16_t index) {
    // The regionCode byte is stored after title, developer, categoryCode and uniqueID
    uint32_t offset = entriesStartOffset + (DB_ENTRY_SIZE * index)
        + sizeof(Entry::title)
        + sizeof(Entry::developer)
        + sizeof(Entry::categoryCode)
        + sizeof(Entry::uniqueID);

    if (!seekTo(offset)) {
        return RegionCode::UNKNOWN;
    }

    RegionCode regionCode = RegionCode::UNKNOWN;

    if (!readBytes(&regionCode, sizeof(regionCode))) {
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
    if (!isDatabaseOpen()) {
        return nullptr;
    }
    
    if (romFile->hasHomebrewGameCode()) {
        return entryForTitle(romFile->title);
    }
    else {
        return entryForUniqueID(romFile->uniqueID, romFile->regionCode, romFile->version);
    }
}

#ifdef N64
void GameDatabase::warmClusterOffsets(const std::vector<GameDatabase::Entry*>& entries) {
    for (GameDatabase::Entry* entry : entries) {
        uint16_t tileIndex = entry->smallLabelTileIndex;
        int tileOffset = getLabelTileOffset(tileIndex);

        // Cache FAT cluster offsets of each label in the database
        f_lseek(&file, tileOffset);
        entry->labelTileCluster = file.clust;
    }

    for (GameDatabase::Entry* entry : entries) {
        if (!entry->hasCheatDatabase()) {
            continue;
        }
        
        f_lseek(&file, cheatsStartOffset + entry->cheatByteOffset);
        entry->cheatsCluster = file.clust;
    }
}

bool GameDatabase::loadCartLabelChunk(Entry* entry, bool highRes, char* buffer, int chunkOffset, int chunkCount) {
    if (!isDatabaseOpen()) {
        return false;
    }

    if (!entry) {
        placeholderEntry.smallLabelTileIndex = 0;
        placeholderEntry.largeLabelTileIndex = 4;

        entry = &placeholderEntry;
    }
    else if (highRes && entry->largeLabelTileIndex == 0) {
        return false;
    }

    uint16_t tileIndex = highRes ? entry->largeLabelTileIndex : entry->smallLabelTileIndex;
    
    tileIndex += chunkOffset;

    size_t size = DB_LABEL_TILE_SIZE * chunkCount;

    int tileByteOffset = getLabelTileOffset(tileIndex);

    int bufferOffset = DB_LABEL_TILE_SIZE * chunkOffset;

    if (entry->labelTileCluster != 0 && !highRes) {
        file.clust = entry->labelTileCluster;
        file.fptr = tileByteOffset;
    }
    else {
        f_lseek(&file, tileByteOffset);

        if (!highRes) {
            entry->labelTileCluster = file.clust;
        }
    }

    UINT bytesRead = 0;

    size_t chunk = size;
    chunk += (DB_LABEL_PADDING_PER_CHUNK * (chunkCount / DB_LABEL_TILES_PER_CHUNK));

    f_read(&file, (void *)(useBSpace ? ROM_ADDRESS_TILES_B : ROM_ADDRESS_TILES_A), chunk, &bytesRead);
    data_cache_hit_writeback_invalidate(buffer + bufferOffset, size);
    dma_read_raw_async(buffer + bufferOffset, (useBSpace ? ROM_ADDRESS_TILES_B : ROM_ADDRESS_TILES_A), size);
    // dma_wait();

    useBSpace = !useBSpace;

    #ifdef N64
    // data_cache_hit_writeback(buffer + bufferOffset, size);
    #endif

    return true;
}
#endif

SpeedrunDatabase* GameDatabase::loadSpeedrunDatabase(Entry* entry) {
    if (!isDatabaseOpen()) {
        return nullptr;
    }

    if (!entry || !entry->hasSpeedrunDatabase()) {
        debugf("[GameDatabase] No speedruns for this entry\n");
        return nullptr;
    }

    long offset = speedrunsStartOffset + (long)entry->speedrunsByteOffset;

    debugf("[GameDatabase] Reading speedruns at local offset: %li\n", (long)entry->speedrunsByteOffset);
    debugf("[GameDatabase] Global offset: %li\n", offset);

    if (!seekTo(offset)) {
        debugf("[GameDatabase] Failed to seek to offset %li\n", offset);
        return nullptr;
    }

    uint32_t addresses[4] = {};

    // TODO: Read straight into database header instead
    if (!readBytes(addresses, sizeof(addresses))) {
        debugf("[GameDatabase] Failed to read speedrun addresses\n");
        return nullptr;
    }

    uint8_t counts[4] = {};

    if (!readBytes(counts, sizeof(counts))) {
        debugf("[GameDatabase] Failed to read speedrun counts\n");
        return nullptr;
    }

    int8_t frameDelayOffset = (int8_t)counts[0];
    uint8_t groupCount = counts[1];
    uint8_t speedrunCount = counts[2];

    SpeedrunDatabase* speedrunDatabase = new SpeedrunDatabase();

    speedrunDatabase->gameCRC1 = addresses[0];
    speedrunDatabase->currentFrameAddress = addresses[1];
    speedrunDatabase->controllerWriteAddress = addresses[2];
    speedrunDatabase->controllerReadAddress = addresses[3];
    speedrunDatabase->frameDelayOffset = frameDelayOffset;

    speedrunDatabase->groups.resize(groupCount);
    speedrunDatabase->entries.resize(speedrunCount);

    bool didReadAll =
        readBytes(speedrunDatabase->groups.data(),  sizeof(SpeedrunGroup) * groupCount)
     && readBytes(speedrunDatabase->entries.data(), sizeof(SpeedrunEntry) * speedrunCount)
    ;

    if (!didReadAll) {
        debugf("[GameDatabase] Failed to read speedrun database\n");

        delete speedrunDatabase;

        return nullptr;
    }

    return speedrunDatabase;
}

CheatDatabase* GameDatabase::loadCheatDatabase(Entry* entry) {
    if (!isDatabaseOpen()) {
        return nullptr;
    }

    if (!entry || !entry->hasCheatDatabase()) {
        debugf("[GameDatabase] No cheats for this entry\n");
        return nullptr;
    }

    long offset = cheatsStartOffset + (long)entry->cheatByteOffset;

    debugf("[GameDatabase] Reading cheats at local offset: %li\n", (long)entry->cheatByteOffset);
    debugf("[GameDatabase] Global offset: %li\n", offset);

    // if (entry->cheatsCluster != 0) {
    //     file.clust = entry->cheatsCluster;
    //     file.fptr = offset;
    // }
    // else
    if (!seekTo(offset)) {
        debugf("[GameDatabase] Failed to seek to offset %li\n", cheatsStartOffset);
        return nullptr;
    }

    uint16_t counts[4] = {};

    // TODO: Read straight into database header instead
    if (!readBytes(counts, sizeof(counts))) {
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
        readBytes(cheatDatabase->groups.data(),          sizeof(CheatGroup)          * groupCount)
     && readBytes(cheatDatabase->cheats.data(),          sizeof(Cheat)               * cheatCount)
     && readBytes(cheatDatabase->codes.data(),           sizeof(CheatCode)           * codeCount)
     && readBytes(cheatDatabase->wildcardOptions.data(), sizeof(CheatWildcardOption) * wildcardOptionsCount)
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
    if (f_open(&file, strip_fs_prefix((char*)filename), FA_READ) != FR_OK) {
        debugf("Failed to open database file: %s\n", filename);
        file = {};
        return false;
    }

    UINT bytesRead = 0;

    f_read(&file, (void *)(ROM_ADDRESS_HEADER), DB_DMA_SIZE, &bytesRead);

    data_cache_hit_writeback_invalidate(headerBuffer, sizeof(headerBuffer));
    dma_read_raw_async(headerBuffer, ROM_ADDRESS_HEADER, sizeof(headerBuffer));
    dma_wait();

    int cursor = 0;

    // READ MAGIC
    // 4 bytes
    cursor += 4;

    if (headerBuffer[0] != 'N' || headerBuffer[1] != 'U' || headerBuffer[2] != 'F' || headerBuffer[3] != 'M') {
        debugf("Invalid database header\n");
        close();
        return false;
    }

    // READ ENTRY COUNT
    // 2 bytes
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

    // READ COMBINED CHEAT DATABASE SIZE
    // 4 bytes
    uint32_t cheatsTotalSize = 0;

    memcpy(&cheatsTotalSize, &headerBuffer[cursor], sizeof(cheatsTotalSize));
    cursor += sizeof(cheatsTotalSize);

    entriesStartOffset = DB_ENTRIES_START_OFFSET;

    labelsStartOffset = entriesStartOffset + (entryCount * DB_ENTRY_SIZE);

    long labelsPadding = (512 - (labelsStartOffset % 512)) % 512;
    labelsStartOffset += labelsPadding;

    // The tiles block has DB_LABEL_PADDING_PER_CHUNK bytes of padding after every
    // DB_LABEL_TILES_PER_CHUNK tiles, so the cheats section starts after the padded
    // end of the tiles block. getLabelTileOffset() applies the same padding math.
    cheatsStartOffset = getLabelTileOffset(tileCount);

    // The cheat databases are written back to back with no padding, so the
    // speedrun section begins directly after the last of them
    speedrunsStartOffset = cheatsStartOffset + cheatsTotalSize;

    debugf("[GameDatabase] Database Entries:       %d\n", entryCount);
    debugf("[GameDatabase] Entries Start Offset:   %li\n", entriesStartOffset);
    debugf("[GameDatabase] Pre-labels Padding:     %li\n", labelsPadding);
    debugf("[GameDatabase] Labels Start Offset:    %li\n", labelsStartOffset);
    debugf("[GameDatabase] Cheats Start Offset:    %li\n", cheatsStartOffset);
    debugf("[GameDatabase] Cheats Total Size:      %lu\n", (unsigned long)cheatsTotalSize);
    debugf("[GameDatabase] Speedruns Start Offset: %li\n", speedrunsStartOffset);

    return true;
}
#endif

void GameDatabase::close() {
    releaseEntriesFromMemory();

    #ifdef N64
    if (isDatabaseOpen()) {
        f_close(&file);
    }

    file = {};
    #endif
}
