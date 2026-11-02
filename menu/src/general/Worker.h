/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <algorithm>

#include <libdragon.h>

#include "boot/boot.h"
#include "flashcart/flashcart.h"
#include "general/ROMFile.h"
#include "utils/utils.h"

class Worker {
    int currentFrame = 0;

    /**
     * Do the actual expensive work per frame.
     */
    virtual void doWork();

protected:
    /**
     * How many units of work a single `advance` gets through. Subclasses raise
     * this to trade a longer frame for a shorter overall wait.
     */
    int workIterationsPerFrame = 1;

public:
    /**
     * Units of work still outstanding -- chunks, blocks, whatever the subclass
     * gets through one `doWork` at a time.
     */
    virtual int workRemaining();

    virtual float progress();

    /**
     * How many `advance` calls the outstanding work needs, which is what a caller
     * pacing itself a frame at a time actually wants to know.
     */
    int framesRemaining() {
        int iterations = workIterationsPerFrame > 0 ? workIterationsPerFrame : 1;
        int remaining = workRemaining();

        // Round up: a trailing part-full frame is still a frame.
        return (remaining + iterations - 1) / iterations;
    }

    void advance() {
        for (int i = 0; i < workIterationsPerFrame; i++) {
            doWork();
        }

        currentFrame++;
    }
};

class LoadROMToCartWorker : public Worker {
    static constexpr unsigned int initialChunk = 11;

    ROMFile romFile;
    flashcart_load_rom_session_t session = {};
    bool hasSession = false;

    void beginSession() {
        if (hasSession) {
            return;
        }

        const FFOBJID* romObject = romFile.hasFileObject() ? &romFile.fileObject : nullptr;

        // byte_swap false: the ROM is staged verbatim, as the sc64 path always did.
        session = flashcart_begin_load_rom_session(romFile.path.data(), romObject, false, initialChunk);
        hasSession = true;
    }

    unsigned int chunksRemaining() {
        if (!hasSession) {
            return (romFile.size + KiB(128) - 1) / KiB(128);
        }

        // Including a session that failed, which closed the file: there's
        // nothing more it can load.
        if (session.is_complete || !flashcart_load_rom_session_valid(&session)) {
            return 0;
        }

        // Covers the flash past the SDRAM portion too, as it's in the ROM size.
        // At least one while the session is open, whatever's left to round.
        size_t loaded = std::min((size_t)session.loaded_bytes, session.rom_size);
        size_t remaining = (session.rom_size - loaded + session.chunk_size - 1) / session.chunk_size;

        return std::max(remaining, (size_t)1);
    }

    void doWork() override {
        beginSession();

        if (!flashcart_load_rom_session_valid(&session)) {
            return;
        }

        flashcart_err_t result = flashcart_load_next_rom_chunk(&session, loadBeginningChunks);

        if (result != FLASHCART_OK) {
            return;
        }

        debugf("[LoadROMToCartWorker] Doing Work. Progress: %f\n", progress());
    }

public:
    bool loadBeginningChunks = false;

    /**
     * Treat every chunk from `initialChunk` on as already loaded, leaving only
     * the beginning chunks the menu lives in. For returning from a warm reset,
     * when the game is still in the cart SDRAM. Call before any work is done.
     */
    void markEndChunksLoaded() {
        beginSession();

        flashcart_mark_end_rom_chunks_loaded(&session);
    }

    int workRemaining() override {
        return (int)chunksRemaining();
    }

    float progress() override {
        return hasSession ? session.progress : 0.0f;
    }

    LoadROMToCartWorker(const ROMFile& romFile) : romFile(romFile) {}
};

class LoadROMFirstMBToMemoryWorker : public Worker {
    // Skip the header and IPL3: the boot path copies this straight to the game's
    // entry point, so it has to hold game code from its very first byte.
    static constexpr uint32_t romAddress = ROM_ADDRESS + GAME_LOAD_ROM_OFFSET;
    static constexpr uint32_t CHUNK_SIZE = KiB(64);
    static constexpr int CHUNK_COUNT = GAME_LOAD_SIZE / CHUNK_SIZE;

    int currentChunk = 0;

    void doWork() override {
        if (!is_memory_expanded() || currentChunk >= CHUNK_COUNT) {
            return;
        }

        uint32_t offset = currentChunk * CHUNK_SIZE;
        uint8_t* destination = (uint8_t*)(GAME_STAGING_ADDRESS + offset);

        data_cache_hit_writeback_invalidate(destination, CHUNK_SIZE);

        dma_read_raw_async(destination, romAddress + offset, CHUNK_SIZE);
        dma_wait();

        debugf("[LoadROMFirstMBToMemoryWorker] Doing Work. Progress: %i / %i\n", currentChunk, CHUNK_COUNT);

        currentChunk++;
    }

public:
    /**
     * Is the whole first MiB staged? Only then can the boot path copy it out of
     * RDRAM instead of going back to the cart.
     */
    bool hasStaged() {
        return currentChunk >= CHUNK_COUNT;
    }

    int workRemaining() override {
        return CHUNK_COUNT - currentChunk;
    }

    float progress() override {
        return (float)currentChunk / CHUNK_COUNT;
    }
};

class LoadROMWorker : public Worker {
    LoadROMToCartWorker cartWorker;
    LoadROMFirstMBToMemoryWorker memoryWorker;

    int totalWork = 0;

    void doWork() override {
        cartWorker.loadBeginningChunks = loadBeginningChunks;

        if (cartWorker.workRemaining() > 0) {
            cartWorker.advance();
            return;
        }

        memoryWorker.advance();
    }

public:
    bool loadBeginningChunks = false;

    LoadROMWorker(const ROMFile& romFile) : cartWorker(romFile) {}

    /**
     * @see LoadROMToCartWorker::markEndChunksLoaded
     */
    void markEndChunksLoaded() {
        cartWorker.markEndChunksLoaded();
    }

    /**
     * @see LoadROMFirstMBToMemoryWorker::hasStaged
     */
    bool hasStagedFirstMB() {
        return memoryWorker.hasStaged();
    }

    /**
     * Is the whole ROM on the cart, ready to boot? Needs `loadBeginningChunks`
     * set to get there.
     */
    bool hasLoadedToCart() {
        return cartWorker.workRemaining() == 0;
    }

    int workRemaining() override {
        return cartWorker.workRemaining() + memoryWorker.workRemaining();
    }

    float progress() override {
        // Deliberately the cart load only, not `workRemaining()`. The staging
        // into RDRAM is opportunistic -- it stops being advanced the moment the
        // caller commits to launching -- so counting it here would peg the bar
        // short of the end for any ROM whose stream outlasts the transition.
        int remaining = cartWorker.workRemaining();

        if (remaining > totalWork) {
            totalWork = remaining;
        }

        if (totalWork == 0) {
            return 1.0f;
        }

        return 1.0f - ((float)remaining / totalWork);
    }
};
