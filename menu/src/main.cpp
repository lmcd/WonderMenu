/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "main.h"

#include <libdragon.h>
#include <t3d/t3d.h>
#include <sys/stat.h>

// Game data and database
#include "flashcart/flashcart.h"
#include "general/GameDatabase.h"
#include "general/GameLibrary.h"

// Scene system
#include "ui/SceneRenderer.h"
#include "ui/scenes/IntroScene.h"
#include "ui/scenes/TestScene.h"

BootSession bootSession;
WarmResetState warmResetState;

uint32_t get_rom_size(void) {
    return io_read(ROM_ADDRESS + 0x18);  // header reserved word, big-endian
}

uint32_t get_entry_point(void) {
    return io_read(ROM_ADDRESS + 0x08);  // header entry point (initial PC), big-endian
}

// We don't ever return from `main`
[[noreturn]]
int main()
{
    int32_t t0 = get_ticks();

    // Init emulator logging
    debug_init_emulog();

    // Init DragonFS (rom:/ filesystem)
    dfs_init(DFS_DEFAULT_LOCATION);

    const char* storagePrefix;

    flashcart_init(&storagePrefix);

    // Restore state from previous launch of WonderMenu, following warm-reset
    warmResetState.restore();

    uint32_t t1 = get_ticks();

    debugf("[main] Base loaded: %.3fs\n", (float)TICKS_TO_MS(t1 - t0) / 1000.0f);

    resolution_t resolution = RESOLUTION_640x480;

    display_init(resolution, DEPTH_16_BPP, BUFF_COUNT, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);

    rdpq_init();

    joypad_init();

    // Init Tiny3D
    t3d_init((T3DInitParams){});  

    // Disable fog
    t3d_fog_set_enabled(false);

    // rdpq_debug_start();

    debugf("[main] ROM Size: %li\n", get_rom_size());
    debugf("[main] ROM Entry Point: 0x%08lX\n", get_entry_point());

    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    GameDatabase database;

    GameLibrary gameLibrary(database, storagePrefix);

    Scene* mainScene;

    if (true) {
        // `IntroScene` loads everything required to run the menu into place.
        // Not the best place to load everything, but it works for now.
        mainScene = new IntroScene(&gameLibrary);
    }
    else {
        mainScene = new TestScene();
    }

    // Create scene renderer
    SceneRenderer sceneRenderer(mainScene);

    // Main render loop
    sceneRenderer.begin();

    // WonderMenu is now finished, and we begin to handover to the selected
    // game.

    t3d_destroy();

    // Record the launched game in the recents list before booting into it.
    if (bootSession.game != nullptr) {
        gameLibrary.addToRecents(GameGroup(*bootSession.game));
    }
    
    // This must be disabled to keep the cart SDRAM writable by the payload.
    // flashcart_deinit();

    bootSession.launch();

    assertf(false, "Unexpected return from 'boot' function");

    while (true) {}
}
