/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstdint>

#include "Region.h"

/**
 * This is the value type for `GameIDMap`, where a 2-character unique game code
 * is the key.
 * 
 * GameIDMap is already a little large, so some effort has been made here to
 * squeeze the offsets of as many game region/variants as possible in 32 bits.
 * 
 * `indexAndCount` holds the index of the game's first entry in its top 10 bits
 * and how many consecutive variants follow it in the low 6. A count of 0 means
 * the cell is empty (no game with that ID).
 *
 * `versionRegionMask` says which version/region variants those entries are.
 */
struct DatabaseLookupEntry {
    static constexpr int COUNT_BITS = 6;
    static constexpr int REGIONS_PER_VERSION = 5;
    static constexpr int VERSION_COUNT = 4;

    static constexpr uint16_t INDEX_MAX = (1 << (16 - COUNT_BITS)) - 1;
    static constexpr uint16_t COUNT_MAX = (1 << COUNT_BITS) - 1;

    // Bit order within each version. Only the most frequently occuring game
    // regions are listed here.
    static constexpr RegionCode REGION_ORDER[REGIONS_PER_VERSION] = {
        RegionCode::JAPAN,
        RegionCode::NORTH_AMERICA,
        RegionCode::EUROPE,
        RegionCode::GERMANY,
        RegionCode::FRANCE
    };

    uint16_t indexAndCount;

    // One bit per (version, region) variant, filled from the most significant
    // bit down:
    // ┌-----------┬-----------┬-----------┬----┐
    // │ v0        │ v1        │ v2        │ v3 │
    // │ J E P D F │ J E P D F │ J E P D F │ J  │
    // │ 0 0 1 1 0 │ 0 0 1 0 0 │ 0 0 0 0 0 │ 0  │ ← Game contains 3 variants
    // └-----------┴-----------┴-----------┴----┘   v1.0 Europe
    //                                              v1.0 Germany
    //                                              v1.1 Europe
    //
    // A set bit means the database holds that variant. The variants are stored
    // consecutively from indexStart() in that same order.
    //
    // `numberOfVariants` can include bits not set here, in which case a manual
    // searxh for the game variant will occur. This is only to optimise the
    // fast-path of frequently occuring regions/versions
    uint16_t versionRegionMask;

    uint16_t indexStart() const {
        return indexAndCount >> COUNT_BITS;
    }
    uint16_t numberOfVariants() const {
        return indexAndCount;
    }

    void set(uint16_t indexStart, uint16_t numberOfVariants) {
        indexAndCount = (uint16_t)((indexStart & INDEX_MAX) << COUNT_BITS)
                      | (numberOfVariants & COUNT_MAX);
    }

    // Bit position of a variant, counting from the most significant bit. Returns
    // -1 when the mask has no room for it: an unlisted region, or any v3 that
    // isn't the first region in REGION_ORDER.
    static int bitIndexFor(uint8_t version, RegionCode regionCode) {
        if (version >= VERSION_COUNT) {
            return -1;
        }

        for (int slot = 0; slot < REGIONS_PER_VERSION; slot++) {
            if (REGION_ORDER[slot] != regionCode) {
                continue;
            }

            int bitIndex = (version * REGIONS_PER_VERSION) + slot;

            return (bitIndex < 16) ? bitIndex : -1;
        }

        return -1;
    }

    static uint16_t maskForBitIndex(int bitIndex) {
        return (uint16_t)(0x8000u >> bitIndex);
    }

    bool hasVariant(uint8_t version, RegionCode regionCode) const {
        int bitIndex = bitIndexFor(version, regionCode);

        return bitIndex >= 0 && (versionRegionMask & maskForBitIndex(bitIndex)) != 0;
    }

    // Database entry index for a variant, or -1 if this game doesn't have it.
    int entryIndexFor(uint8_t version, RegionCode regionCode) const {
        int bitIndex = bitIndexFor(version, regionCode);

        if (bitIndex < 0) {
            return -1;
        }

        uint16_t bit = maskForBitIndex(bitIndex);

        if ((versionRegionMask & bit) == 0) {
            return -1;
        }

        // Every set bit above this one takes an earlier consecutive entry, so
        // counting them gives the offset from indexStart(). Widened to 32 bits so
        // the shift can't drop the top bit when bitIndex is 0.
        uint16_t above = (uint16_t)(versionRegionMask & ~(((uint32_t)bit << 1) - 1));

        return indexStart() + __builtin_popcount(above);
    }

    void setVariant(uint8_t version, RegionCode regionCode) {
        int bitIndex = bitIndexFor(version, regionCode);

        if (bitIndex >= 0) {
            versionRegionMask |= maskForBitIndex(bitIndex);
        }
    }
};

static_assert(sizeof(DatabaseLookupEntry) == 4, "DatabaseLookupEntry must be 4 bytes");
