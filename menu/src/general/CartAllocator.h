/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <libdragon.h>
#include "utils/utils.h"
#include "../../shared/src/PayloadData.h"

struct CartAllocator {
    enum Region {
        // Where WonderMenu lives
        REGION_MENU,
        // Where a loaded game lives
        REGION_ROM,
        // Screenshots accumulated during a game session
        REGION_SCREENSHOTS,
        // Speedrun recorded by the user that's written to cart on pre-NMI
        REGION_SPEEDRUN,
        // Where `WarmResetState` is stored
        REGION_WARM_RESET_STATE,
        // Scratch space for database entries, before they're moved to RAM
        REGION_DATABASE,
        // Scratch space for loading tiles
        REGION_TILES_A,
        // Scratch space for loading tiles
        REGION_TILES_B
    };

    enum Alignment {
        // Allocation grows from the left
        ALIGN_LEFT,
        // Allocation grows from the right
        ALIGN_RIGHT
    };

    struct Allocation {
        int offset = 0;
        size_t maxSize = 0;
        bool isValid = true;

        uint32_t romAddress() {
            return ROM_ADDRESS + offset;
        }
        
        void write(const void* source, size_t localOffset, size_t size) {
            assertf(isValid, "Writing to an invalidated allocation");
            assertf(size <= maxSize, "Writing %zu bytes to a %zu byte allocation", size, maxSize);

            data_cache_hit_writeback(source, size);
            dma_write_raw_async(source, romAddress() + localOffset, size);
            dma_wait();
        }

        void read(void* destination, size_t localOffset, size_t size) {
            assertf(isValid, "Reading from an invalidated allocation");
            assertf(size <= maxSize, "Reading %zu bytes from a %zu byte allocation", size, maxSize);

            data_cache_hit_writeback_invalidate(destination, size);
            dma_read_raw_async(destination, romAddress() + localOffset, size);
            dma_wait();
        }

        void ioWrite(size_t byteOffset, uint32_t value) {
            assertf(isValid, "Writing to an invalidated allocation");
            assertf(byteOffset + sizeof(uint32_t) <= maxSize, "Writing a word at %zu in a %zu byte allocation", byteOffset, maxSize);

            io_write(romAddress() + byteOffset, value);
        }

        uint32_t ioRead(size_t byteOffset) {
            assertf(isValid, "Reading from an invalidated allocation");
            assertf(byteOffset + sizeof(uint32_t) <= maxSize, "Reading a word at %zu in a %zu byte allocation", byteOffset, maxSize);

            return io_read(romAddress() + byteOffset);
        }

        void invalidate() {
            isValid = false;
        }
    };

    static constexpr int SDRAM_CAPACITY = MiB(64) - KiB(128);
    static constexpr int MAX_ALLOCATIONS = 20;

    Allocation allocations[MAX_ALLOCATIONS];
    int allocationCount = 0;

    static size_t romSize() {
        return io_read(ROM_ADDRESS + 0x18);
    }

    static int alignedSpan(size_t size) {
        return (int)ALIGN(size, 16);
    }

    const Allocation* findOverlap(int offset, int span) const {
        for (int i = 0; i < allocationCount; i++) {
            const Allocation& other = allocations[i];

            if (other.isValid && offset < other.offset + alignedSpan(other.maxSize) && other.offset < offset + span) {
                return &other;
            }
        }

        return nullptr;
    }

    Allocation allocate(Region, Alignment alignment, size_t maxSize) {
        Allocation allocation;
        allocation.maxSize = maxSize;
        allocation.isValid = false;

        int span = alignedSpan(maxSize);

        if (allocationCount == MAX_ALLOCATIONS || span > SDRAM_CAPACITY) {
            return allocation;
        }

        int offset = (alignment == ALIGN_LEFT) ? 0 : SDRAM_CAPACITY - span;

        while (offset >= 0 && offset + span <= SDRAM_CAPACITY) {
            const Allocation* overlap = findOverlap(offset, span);

            if (overlap == nullptr) {
                allocation.offset = offset;
                allocation.isValid = true;
                allocations[allocationCount++] = allocation;

                return allocation;
            }

            offset = (alignment == ALIGN_LEFT)
                ? overlap->offset + alignedSpan(overlap->maxSize)
                : overlap->offset - span;
        }

        return allocation;
    }

    // Allocation forceAllocate(Region region, Alignment alignment, int offset, size_t maxSize) {
    //     return Allocation();
    // }
};
