/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "main.h"

#include <libdragon.h>
#include <t3d/t3d.h>

#include "flashcart/flashcart.h"
#include "general/GameDatabase.h"
#include "general/GameLibrary.h"

// Scene system
#include "general/CartAllocator.h"
#include "rdpq_text.h"
#include "ui/SceneRenderer.h"
#include "ui/scenes/IntroScene.h"
#include "ui/scenes/TestScene.h"

BootSession bootSession;
WarmResetState warmResetState;
// CartAllocator cartAllocator;

// We don't ever return from `main`
[[noreturn]]
int main()
{
    // using enum CartAllocator::Region;
    // using enum CartAllocator::Alignment;

    // cartAllocator.allocate(
    //     REGION_MENU,
    //     ALIGN_LEFT,
    //     CartAllocator::romSize()
    // );
    // cartAllocator.allocate(
    //     REGION_TILES_A, 
    //     ALIGN_LEFT,
    //     DB_L_LABEL_SIZE
    // );
    // cartAllocator.allocate(
    //     REGION_TILES_B, 
    //     ALIGN_LEFT,
    //     DB_L_LABEL_SIZE
    // );
    // cartAllocator.allocate(
    //     REGION_DATABASE,
    //     ALIGN_LEFT,
    //     KiB(128)
    // );
    // cartAllocator.allocate(
    //     REGION_WARM_RESET_STATE,
    //     ALIGN_RIGHT,
    //     WARM_RESET_STATE_DATA_SIZE
    // );

    // Init emulator logging
    debug_init_emulog();

    // Init DragonFS (rom:/ filesystem)
    dfs_init(DFS_DEFAULT_LOCATION);

    const char* storagePrefix;

    flashcart_init(&storagePrefix);

    // Restore state from previous launch of WonderMenu, following warm-reset
    warmResetState.restore();

    display_init(
        RESOLUTION_640x480, 
        DEPTH_16_BPP, 
        FRAMEBUFFER_COUNT, 
        GAMMA_NONE, 
        FILTERS_RESAMPLE_ANTIALIAS_DEDITHER
    );

    rdpq_init();

    joypad_init();

    // Init Tiny3D
    t3d_init((T3DInitParams){});  

    // Disable fog
    t3d_fog_set_enabled(false);

    // Enabled RDPQ validator
    // rdpq_debug_start();

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
