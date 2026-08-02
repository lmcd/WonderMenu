/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "main.h"

#include <libdragon.h>
#include <t3d/t3d.h>
#include <sys/stat.h>

// Game data and database
#include "general/GameDatabase.h"
#include "general/GameLibrary.h"

// Scene system
#include "ui/SceneRenderer.h"
#include "ui/scenes/IntroScene.h"
#include "ui/scenes/TestScene.h"

GameLaunchSession gameLaunchSession;

uint32_t get_rom_size(void) {
    return io_read(0x10000000 + 0x18);  // header reserved word, big-endian
}

uint32_t get_entry_point(void) {
    return io_read(0x10000000 + 0x08);  // header entry point (initial PC), big-endian
}

// We don't ever return from `main`
[[noreturn]]
int main()
{
    int32_t t0 = get_ticks();

    // Init emulator logging
    debug_init_emulog();

    // Init USB logging
    debug_init_usblog();

    // Init DragonFS (rom:/ filesystem)
    dfs_init(DFS_DEFAULT_LOCATION);

    // Init sfds (sd:/ filesystem)
    debug_init_sdfs("sd:/", -1);

    const char* storagePrefix;

    flashcart_init(&storagePrefix);

    uint32_t t1 = get_ticks();

    debugf("[main] Base loaded: %.3fs\n", (float)TICKS_TO_MS(t1 - t0) / 1000.0f);

    if (strcmp(storagePrefix, "rom:/") == 0) {
        storagePrefix = "rom:/sd/";
    }

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

    IntroScene* introScene = new IntroScene(&gameLibrary);

	// TestScene* testScene = new TestScene();

    // Create scene renderer
    SceneRenderer sceneRenderer(introScene);

    // Main render loop
    sceneRenderer.begin();

    t3d_destroy();

    // Record the launched game in the recents list before booting into it.
    if (gameLaunchSession.game != nullptr) {
        gameLibrary.addToRecents(GameGroup(*gameLaunchSession.game));

        gameLibrary.lastLaunchedGame = gameLaunchSession.game;
        gameLibrary.writeCacheHeader();
    }
    
    flashcart_deinit();

    gameLaunchSession.launch();

    assertf(false, "Unexpected return from 'boot' function");

    while (true) {}
}
