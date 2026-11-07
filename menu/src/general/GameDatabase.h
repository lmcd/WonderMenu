/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "utils/utils.h"
#include <cstdint>
#ifdef N64
#include <libdragon.h>
#include <fatfs/ff.h>
extern "C" {
#include "utils/fs.h"
}
#endif
#include <cstddef>
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
constexpr size_t DB_ENTRY_RESERVED_LENGTH = 2;

// TODO: don't hardcode
constexpr int DB_STRINGS_START_OFFSET = 5224;

constexpr int DB_DMA_SIZE = KiB(112);

/**
 * Represents the WonderMenu.db database format.
 */
class GameDatabase {
public:
    /**
     * Database entry for a known game
     */
    struct Entry {
        struct __attribute__((packed)) Data {
            // Byte offsets into the string pool
            uint16_t titleOffset;
            uint16_t developerOffset;

            // An alphanumeric character that specifies the kind of media the game is stored on.
            CategoryCode categoryCode;

            // The 2 character game ID (e.g. BK)
            char uniqueID[2];

            RegionCode regionCode;

            // Share a byte: saveType in the high nibble, cicType in the low
            uint8_t saveType : 4;
            uint8_t cicType  : 4;

            uint16_t smallLabelTileIndex;
            uint16_t largeLabelTileIndex;

            // Byte offset into cheat data section
            // 0xFFFFFFFF = no cheats
            uint32_t cheatByteOffset;

            // Byte offset into speedrun data section
            // 0xFFFFFFFF = no speedruns
            uint32_t speedrunsByteOffset;

            bool requiresExpansionPak;

            // Spare room for fields added later
            uint8_t reserved[DB_ENTRY_RESERVED_LENGTH];
        };

        Data& data;
        const char* strings;

        uint32_t labelTileCluster = 0;
        uint32_t largeLabelTileCluster = 0;
        uint32_t cheatsCluster = 0;
        uint16_t entryIndex;

        Entry(Data& data, const char* strings, uint16_t entryIndex) : data(data), strings(strings), entryIndex(entryIndex) {}

        // The title of the game (e.g. Banjo Kazooie)
        const char* title() const {
            return strings + data.titleOffset;
        }

        // The developer of the game (e.g. Rareware)
        const char* developer() const {
            return strings + data.developerOffset;
        }

        bool hasSpeedrunDatabase() {
            return data.speedrunsByteOffset != NO_SPEEDRUNS_DATABASE;
        }

        bool hasCheatDatabase() {
            return data.cheatByteOffset != NO_CHEATS_DATABASE;
        }

        bool hasHomebrewGameCode() {
            // Homebrew developed with libdragon has a 'ED' unique ID
            if (data.uniqueID[0] == 'E' && data.uniqueID[1] == 'D') {
                return true;
            }
            // Some Homebrew titles appear to have NULL bytes here
            else if (data.uniqueID[0] == 0 && data.uniqueID[1] == 0) {
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

    Entry::Data placeholderData = {};
    Entry placeholderEntry{placeholderData, "", 0};

    #ifdef N64
    // Zeroed so `obj.fs` reads as null (not open) until load() opens it
    FIL file = {};
    #endif

    /**
     * Number of `Entry` records in the database, read from the header.
     */
    uint16_t entryCount = 0;

    uint16_t stringsSize = 0;

    // The string pool, with the entries directly after it in the same block
    char* strings = nullptr;
    Entry::Data* entriesData = nullptr;

    long entriesStartOffset;
    long labelsStartOffset;
    long cheatsStartOffset;
    long speedrunsStartOffset;

    bool useBSpace = false;

    bool seekTo(long offset);
    bool readBytes(void* buffer, size_t size);

    bool loadEntriesIntoMemory();
    void releaseEntriesFromMemory();

public:
    GameDatabase();
    ~GameDatabase();

    static uint32_t djb2(const unsigned char* data, size_t length);

    bool isOpen() const;

    int getLabelTileOffset(int tileIndex);

    Entry* readEntryAtIndex(uint16_t index);

    /**
     * Read only the region code for the entry at the given index.
     * Cheaper than readEntryAtIndex when scanning variants, as it avoids
     * allocating an entry.
     * Returns RegionCode::UNKNOWN for an index outside the database.
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

/**
 * Size in bytes of each game entry in the database
 */
constexpr size_t DB_ENTRY_SIZE = sizeof(GameDatabase::Entry::Data);

static_assert(DB_ENTRY_SIZE == 24, "DB_ENTRY_SIZE must stay at 24 bytes");

static_assert(DB_ENTRY_SIZE % 8 == 0, "DB_ENTRY_SIZE must stay a multiple of 8");

// Every entry must begin on a 2-byte boundary, which single-entry PI DMA requires
static_assert(DB_ENTRY_SIZE % 2 == 0, "DB_ENTRY_SIZE must stay even for PI DMA");
