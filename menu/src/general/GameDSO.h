/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <libdragon.h>

#include "dso/game_state.hpp"

/**
 * A game's state DSO (rom:/state/<code>.dso, see dso/game_state_abi.h), open
 * for as long as this is. <code> is the cart ID in uppercase, e.g. "SM" for
 * NSME. Only usable while the menu's ROM is in cart SDRAM.
 */
struct GameDSO {
    void* handle = nullptr;

    GameDSO() = default;
    GameDSO(const GameDSO&) = delete;
    GameDSO& operator=(const GameDSO&) = delete;

    ~GameDSO() {
        close();
    }

    /**
     * Opens the DSO for a cart ID. False if the game has none, which is most of
     * them.
     */
    bool open(const char uniqueID[2]) {
        close();

        unsigned char id0 = (unsigned char)uniqueID[0];
        unsigned char id1 = (unsigned char)uniqueID[1];

        if (!isalnum(id0) || !isalnum(id1)) {
            return false;
        }

        char dfsPath[16];
        snprintf(dfsPath, sizeof(dfsPath), "state/%c%c.dso", toupper(id0), toupper(id1));

        uint32_t romAddress = dfs_rom_addr(dfsPath);
        
        // dlopen asserts on a missing file
        if (romAddress == 0) {
            debugf("[GameDSO] No state DSO %s\n", dfsPath);
            return false;
        }

        char path[24];
        snprintf(path, sizeof(path), "rom:/%s", dfsPath);

        handle = dlopen(path, RTLD_LOCAL);

        if (handle == nullptr) {
            debugf("[GameDSO] Couldn't open %s: %s\n", path, dlerror());
            return false;
        }

        return true;
    }

    void close() {
        if (handle == nullptr) {
            return;
        }
        
        dlclose(handle);
        handle = nullptr;
    }

    bool isOpen() const {
        return handle != nullptr;
    }

    void* symbol(const char* name) const {
        return (handle != nullptr) ? dlsym(handle, name) : nullptr;
    }

    /**
     * Copies a function's code out of the DSO to `image`, as many bytes as its
     * size symbol (`name` + GAME_STATE_SIZE_SYMBOL_SUFFIX) says, if it has
     * both and it fits, and writes it out to RDRAM and out of the I-cache so
     * it can be run from there. Returns how many bytes were copied: 0 if it
     * didn't.
     */
    uint32_t copyCode(const char* name, uint8_t* image, uint32_t capacity) const {
        char sizeName[64];
        snprintf(sizeName, sizeof(sizeName), "%s" GAME_STATE_SIZE_SYMBOL_SUFFIX, name);

        const void* code = symbol(name);
        const uint32_t* size = (const uint32_t*)symbol(sizeName);

        if (code == nullptr || size == nullptr || *size == 0 || *size > capacity) {
            return 0;
        }

        memcpy(image, code, *size);

        data_cache_hit_writeback_invalidate(image, *size);
        // It's run as code
        inst_cache_hit_invalidate(image, *size);

        return *size;
    }

    game_state::TitleFunction titleFunction() const {
        return (game_state::TitleFunction)symbol(GAME_STATE_TITLE_SYMBOL);
    }

    game_state::StatsFunction statsFunction() const {
        return (game_state::StatsFunction)symbol(GAME_STATE_STATS_SYMBOL);
    }

    game_state::SpriteFunction spriteFunction() const {
        return (game_state::SpriteFunction)symbol(GAME_STATE_SPRITE_SYMBOL);
    }

    game_state::RenderRectFunction renderRectFunction() const {
        return (game_state::RenderRectFunction)symbol(GAME_STATE_RENDER_RECT_SYMBOL);
    }
};
