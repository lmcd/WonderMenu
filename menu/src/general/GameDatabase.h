/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "utils/utils.h"
#ifdef N64
#include <libdragon.h>
#include <fatfs/ff.h>
extern "C" {
#include "utils/fs.h"
}
#endif
#include <cstring>
#include <vector>
#include <unordered_map>

#include "CheatDatabase.h"
#include "DatabaseLookupEntry.h"
#include "GameIdentifier.h"
#include "GameIDMap.h"
#include "Region.h"
#include "ROMFile.h"
#include "SpeedrunDatabase.h"

/**
 * Width in pixels of each label tile.
 */
constexpr int DB_LABEL_TILE_WIDTH  = 38;

/**
 * Height in pixels of each label tile.
 */
constexpr int DB_LABEL_TILE_HEIGHT = 44;

/**
 * Number of tiles used to display a small cartridge label.
 */
constexpr int DB_S_LABEL_TILE_COUNT = 4; 

/**
 * Number of tiles used to display a large cartridge label.
 */
constexpr int DB_L_LABEL_TILE_COUNT = 16; 

/**
 * Size in bytes of each label tile.
 */
constexpr int DB_LABEL_TILE_SIZE = (DB_LABEL_TILE_WIDTH * DB_LABEL_TILE_HEIGHT * 2);

/**
 * How many tiles can be fetched at once.
 */
constexpr int DB_LABEL_TILES_PER_CHUNK = 4;

constexpr int DB_LABEL_CHUNKS_PER_S_LABEL = DB_S_LABEL_TILE_COUNT / DB_LABEL_TILES_PER_CHUNK;
constexpr int DB_LABEL_CHUNKS_PER_L_LABEL = DB_L_LABEL_TILE_COUNT / DB_LABEL_TILES_PER_CHUNK;

/**
 * Number of bytes to pad to take DB_LABEL_TILE_SIZE to a multiple of 512.
 */
constexpr int DB_LABEL_PADDING_PER_CHUNK = (512 - ((DB_LABEL_TILE_SIZE * DB_LABEL_TILES_PER_CHUNK) % 512)) % 512;

constexpr int DB_LABEL_CHUNK_SIZE = (DB_LABEL_TILES_PER_CHUNK * DB_LABEL_TILE_SIZE) + DB_LABEL_PADDING_PER_CHUNK;

/**
 * Size in bytes of each small game label in the database.
 */
constexpr size_t DB_S_LABEL_SIZE = DB_LABEL_TILE_SIZE * DB_S_LABEL_TILE_COUNT;

/**
 * Size in bytes of each large game label in the database.
 */
constexpr size_t DB_L_LABEL_SIZE = DB_LABEL_TILE_SIZE * DB_L_LABEL_TILE_COUNT;

constexpr size_t DB_S_LABEL_SIZE_PADDED = DB_S_LABEL_SIZE + (DB_LABEL_CHUNKS_PER_S_LABEL * DB_LABEL_PADDING_PER_CHUNK);
constexpr size_t DB_L_LABEL_SIZE_PADDED = DB_L_LABEL_SIZE + (DB_LABEL_CHUNKS_PER_L_LABEL * DB_LABEL_PADDING_PER_CHUNK);

constexpr size_t DB_ENTRY_TITLE_LENGTH = 64;
constexpr size_t DB_ENTRY_DEVELOPER_LENGTH = 32;

/**
 * Spare bytes padded to the end of every entry.
 */
constexpr size_t DB_ENTRY_RESERVED_LENGTH = 6;

/**
 * Size in bytes of each game entry in the database
 */
constexpr size_t DB_ENTRY_SIZE = DB_ENTRY_TITLE_LENGTH + DB_ENTRY_DEVELOPER_LENGTH + 4 + 1 + 1 + 2 + 2 + 4 + 4
                               + DB_ENTRY_RESERVED_LENGTH;

static_assert(DB_ENTRY_SIZE == 120, "DB_ENTRY_SIZE must stay at 120 bytes");

// Every entry must begin on a 2-byte boundary, which single-entry PI DMA requires
static_assert(DB_ENTRY_SIZE % 2 == 0, "DB_ENTRY_SIZE must stay even for PI DMA");

// TODO: don't hardcode
constexpr int DB_ENTRIES_START_OFFSET = 5224;

constexpr int DB_DMA_SIZE = KiB(112);
// constexpr int DB_DMA_SIZE = DB_ENTRIES_START_OFFSET + (DB_ENTRY_SIZE * 906);

/**
 * Represents the WonderMenu.db database format.
 */
class GameDatabase {
public:
    /**
     * Database entry for a known game
     */
    struct __attribute__((packed)) Entry {
        // The title of the game (e.g. Banjo Kazooie)
        char title[DB_ENTRY_TITLE_LENGTH];

        // The developer of the game (e.g. Rareware)
        char developer[DB_ENTRY_DEVELOPER_LENGTH];

        // An alphanumeric character that specifies the kind of media the game is stored on.
        CategoryCode categoryCode;

        // The 2 character game ID (e.g. BK)
        char uniqueID[2] = "0";

        RegionCode regionCode;

        SaveType saveType;

        uint8_t cicType;

        uint16_t smallLabelTileIndex;
        uint16_t largeLabelTileIndex;

        // Byte offset into cheat data section
        // 0xFFFFFFFF = no cheats
        uint32_t cheatByteOffset;

        // Byte offset into speedrun data section
        // 0xFFFFFFFF = no speedruns
        uint32_t speedrunsByteOffset;

        // Spare room for fields added later
        uint8_t reserved[DB_ENTRY_RESERVED_LENGTH];

        // Not imported from .db file:

        uint32_t labelTileCluster = 0;
        uint32_t cheatsCluster = 0;
        uint16_t entryIndex;

        bool hasSpeedrunDatabase() {
            return speedrunsByteOffset != NO_SPEEDRUNS_DATABASE;
        }

        bool hasCheatDatabase() {
            return cheatByteOffset != NO_CHEATS_DATABASE;
        }

        bool hasHomebrewGameCode() {
            // Homebrew developed with libdragon has a 'ED' unique ID
            if (uniqueID[0] == 'E' && uniqueID[1] == 'D') {
                return true;
            }
            // Some Homebrew titles appear to have NULL bytes here
            else if (uniqueID[0] == 0 && uniqueID[1] == 0) {
                return true;
            }
            else {
                return false;
            }
        }
    };

private:
    GameIDMap<DatabaseLookupEntry> gameIDMap;

    std::unordered_map<uint32_t, uint16_t> titleLookupMap;

    Entry placeholderEntry;

    #ifdef N64
    // Zeroed so `obj.fs` reads as null (not open) until load() opens it
    FIL file = {};
    #endif

    /**
     * Number of `Entry` records in the database, read from the header.
     */
    uint16_t entryCount = 0;

    /**
     * The whole entries block held in RDRAM, `entryCount` records of
     * `DB_ENTRY_SIZE` bytes each. Null until `loadEntriesIntoMemory()` is
     * called, which is what `readEntryAtIndex` keys its fast path off.
     */
    unsigned char* entriesData = nullptr;

    long entriesStartOffset;
    long labelsStartOffset;
    long cheatsStartOffset;
    long speedrunsStartOffset;

    bool useBSpace = false;

    bool seekTo(long offset);
    bool readBytes(void* buffer, size_t size);

public:
    GameDatabase();
    ~GameDatabase();

    static uint32_t djb2(const unsigned char* data, size_t length);

    bool isDatabaseOpen();

    int getLabelTileOffset(int tileIndex);

    Entry* readEntryAtIndex(uint16_t index);

    /**
     * On `load()`, entries are moved from the SD card into cartridge SDRAM.
     * This method moves the entries again from cart space in main memory.
     * This provides a fast path for database seeking on boot.
     */
    bool loadEntriesIntoMemory();

    /**
     * Release memory temporarily occupied by database entries during boot.
     */
    void releaseEntriesFromMemory();

    bool hasEntriesInMemory() const { return entriesData != nullptr; }

    /**
     * Read only the region code for the entry at the given index.
     * Cheaper than readEntryAtIndex when scanning variants, as it avoids
     * allocating and reading the full entry.
     * Returns RegionCode::UNKNOWN on read failure.
     */
    RegionCode readRegionCodeAtIndex(uint16_t index);

    int16_t indexForTitle(char title[21]);
    int16_t indexForUniqueID(char uniqueID[2], RegionCode regionCode, uint8_t version);

    Entry* entryForTitle(char title[21]);
    Entry* entryForUniqueID(char uniqueID[2], RegionCode regionCode, uint8_t version);

    /**
     * Get database entry for a game ID
     * Returns nullptr if not found
     */
    Entry* entryForROMFile(ROMFile* romFile);

    /**
     * Load database from WonderMenu.db
     * Reads raw Entry structs and populates the database
     */
    #ifdef N64
    bool load(const char* filename);
    #endif

    void close();

    #ifdef N64
    void warmClusterOffsets(const std::vector<GameDatabase::Entry*>& entries);

    bool loadCartLabelChunk(Entry* entry, bool highRes, char* buffer, int chunkOffset, int chunkCount);
    #endif

    /**
     * Load the CheatDatabase for a given database entry.
     */
    CheatDatabase* loadCheatDatabase(Entry* entry);

    /**
     * Load the SpeedrunDatabase for a given database entry, or `nullptr` when
     * the entry has none. The caller owns the result.
     */
    SpeedrunDatabase* loadSpeedrunDatabase(Entry* entry);
};
