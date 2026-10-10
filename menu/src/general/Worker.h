/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

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
    static constexpr unsigned int initialChunk = 10;

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

        unsigned int total = (session.sdram_size + session.chunk_size - 1) / session.chunk_size;
        unsigned int loaded = session.loaded_bytes / session.chunk_size;

        return loaded >= total ? 0 : (total - loaded);
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
     * Load whatever is left of the ROM in one blocking go, and close the
     * session. Does nothing if the session was never opened.
     *
     * @param progressHandler Invoked as the remaining chunks land, so the caller
     *                        can keep drawing while the load blocks. Optional.
     */
    flashcart_err_t finish(flashcart_progress_callback_t* progressHandler = nullptr) {
        if (!hasSession) {
            return FLASHCART_OK;
        }

        return flashcart_finish_load_rom_session(&session, progressHandler);
    }

    int workRemaining() override {
        return (int)chunksRemaining();
    }

    float progress() override {
        return hasSession ? session.progress : 0.0f;
    }

    LoadROMToCartWorker(const ROMFile& romFile) : romFile(romFile) {}

    flashcart_load_rom_session_t* loadSession() {
        return hasSession ? &session : nullptr;
    }
};

class LoadROMFirstMBToMemoryWorker : public Worker {
    // Skip the header and IPL3: the boot path copies this straight to the game's
    // entry point, so it has to hold game code from its very first byte.
    static constexpr uint32_t romAddress = ROM_ADDRESS + GAME_LOAD_ROM_OFFSET;
    static constexpr uint32_t stagingAddress = GAME_STAGING_ADDRESS;
    static constexpr uint32_t chunkSize = KiB(64);
    static constexpr int chunkCount = GAME_LOAD_SIZE / chunkSize;

    int currentChunk = 0;

    void doWork() override {
        if (!is_memory_expanded() || currentChunk >= chunkCount) {
            return;
        }

        uint32_t offset = currentChunk * chunkSize;
        uint8_t* destination = (uint8_t*)(stagingAddress + offset);

        data_cache_hit_writeback_invalidate(destination, chunkSize);

        dma_read_raw_async(destination, romAddress + offset, chunkSize);
        dma_wait();

        debugf("[LoadROMFirstMBToMemoryWorker] Doing Work. Progress: %i / %i\n", currentChunk, chunkCount);

        currentChunk++;
    }

public:
    /**
     * Is the whole first MiB staged? Only then can the boot path copy it out of
     * RDRAM instead of going back to the cart.
     */
    bool hasStaged() {
        return currentChunk >= chunkCount;
    }

    int workRemaining() override {
        return chunkCount - currentChunk;
    }

    float progress() override {
        return (float)currentChunk / chunkCount;
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
     * @see LoadROMFirstMBToMemoryWorker::hasStaged
     */
    bool hasStagedFirstMB() {
        return memoryWorker.hasStaged();
    }

    /**
     * @see LoadROMToCartWorker::finish
     */
    flashcart_err_t finish(flashcart_progress_callback_t* progressHandler = nullptr) {
        return cartWorker.finish(progressHandler);
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

    flashcart_load_rom_session_t* loadSession() {
        return cartWorker.loadSession();
    }
};
