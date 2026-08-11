/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

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

/**
 * Width in pixels of each label tile segment
 */
constexpr int DB_LABEL_TILE_WIDTH  = 38;
/**
 * Height in pixels of each label tile segment
 */
constexpr int DB_LABEL_TILE_HEIGHT = 44;
/**
 * Number of tiles used to display a small cartridge label
 */
constexpr int DB_S_LABEL_TILE_COUNT = 4; 
/**
 * Number of tiles used to display a large cartridge label
 */
constexpr int DB_L_LABEL_TILE_COUNT = 16; 
/**
 * Size in bytes of each label tile segment
 */
constexpr int DB_LABEL_TILE_SIZE = (DB_LABEL_TILE_WIDTH * DB_LABEL_TILE_HEIGHT * 2);
/**
 * How many tiles can be fetched at once
 */
constexpr int DB_LABEL_TILES_PER_CHUNK = 4;
constexpr int DB_LABEL_CHUNKS_PER_S_LABEL = DB_S_LABEL_TILE_COUNT / DB_LABEL_TILES_PER_CHUNK;
constexpr int DB_LABEL_CHUNKS_PER_L_LABEL = DB_L_LABEL_TILE_COUNT / DB_LABEL_TILES_PER_CHUNK;

/**
 * Number of bytes to pad to take DB_LABEL_TILE_SIZE to a multuple of 512
 */
constexpr int DB_LABEL_PADDING_PER_CHUNK = (512 - ((DB_LABEL_TILE_SIZE * DB_LABEL_TILES_PER_CHUNK) % 512)) % 512;

constexpr int DB_LABEL_CHUNK_SIZE = (DB_LABEL_TILES_PER_CHUNK * DB_LABEL_TILE_SIZE) + DB_LABEL_PADDING_PER_CHUNK;
/**
 * Size in bytes of each game entry in the database
 */
constexpr size_t DB_ENTRY_SIZE = 50 + 50 + 4 + 1 + 2 + 2 + 4 + 1;
/**
 * Size in bytes of each small game label in the database
 */
constexpr size_t DB_S_LABEL_SIZE = DB_LABEL_TILE_SIZE * DB_S_LABEL_TILE_COUNT;
/**
 * Size in bytes of each large game label in the database
 */
constexpr size_t DB_L_LABEL_SIZE = DB_LABEL_TILE_SIZE * DB_L_LABEL_TILE_COUNT;

constexpr size_t DB_S_LABEL_SIZE_PADDED = DB_S_LABEL_SIZE + (DB_LABEL_CHUNKS_PER_S_LABEL * DB_LABEL_PADDING_PER_CHUNK);
constexpr size_t DB_L_LABEL_SIZE_PADDED = DB_L_LABEL_SIZE + (DB_LABEL_CHUNKS_PER_L_LABEL * DB_LABEL_PADDING_PER_CHUNK);

// TODO: don't hardcode
constexpr int DB_ENTRIES_START_OFFSET = 5224;

/**
 * Parses the WonderMenu.db format
 * The database contains a list of all games released for the N64 with various metadata:
 *  - Title
 *  - Developer
 *  - Unique Game ID
 *  - Binary Label Data
 */
class GameDatabase {
public:
    /**
     * Database entry for a known game
     */
    struct __attribute__((packed)) Entry {
        // The title of the game (e.g. Banjo Kazooie)
        char title[50];

        // The developer of the game (e.g. Rareware)
        char developer[50];

        // An alphanumeric character that specifies the kind of media the game is stored on.
        // N  Game Pak
        // D  64DD Disk
        // C  Expandable Game: Game Pak Part
        // E  Expandable Game: 64DD Disk Part
        // Z  Aleck64 Game Pak
        CategoryCode categoryCode;

        // The 2 character game ID (e.g. BK)
        char uniqueID[2];

        RegionCode regionCode;

        SaveType saveType;

        uint16_t smallLabelTileIndex = 0;
        uint16_t largeLabelTileIndex = 0;
        uint32_t cheatByteOffset;  // byte offset into cheat data section, 0xFFFFFFFF = no cheats

        uint8_t supportsSpeedruns = 0;

        // Not imported from .db file:
        uint32_t labelTileCluster = 0;
        uint16_t entryIndex;

        Entry();

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
    
    // The file handler pointing to WonderMenu.db
    // We read from this during the lifetime of `GameDatabase`
    FILE* databaseFile;
    
    Entry placeholderEntry;

    #ifdef N64
    FIL fil;
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

    bool useBSpace = false;

public:
    GameDatabase();
    ~GameDatabase();

    static uint32_t djb2(const unsigned char* data, size_t length);

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

    void warmLabelClusterOffset(Entry* entry);

    #ifdef N64
    bool loadCartLabelChunk(Entry* entry, bool highRes, char* buffer, int chunkOffset, int chunkCount);
    #endif

    /**
     * Load the CheatDatabase for a given database entry
     */
    CheatDatabase* loadCheatDatabase(Entry* entry);
};
