/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>

#include "general/GameDatabase.h"
#include "general/LabelLoader.h"
#include "ui/SceneRenderer.h"
#include "util/Color.h"
#include "util/Rect.h"
#include "util/Size.h"
#include "util/Vec2.h"
#include "util/Vec3.h"

#define CART_MODEL_SIZE Size(13568, 8768)
#define DISTANCE_FROM_CAMERA 50.0f

template<typename F>
rspq_block_t* rspq_make_block(F&& f) {
    rspq_block_begin();
    f();
    return rspq_block_end();
}

struct PendingCartPreload {
    // cacheIndex -1 means nothing is pending; the defaults spell out the empty
    // state so callers don't have to fill in trailing members to get it.
    int cacheIndex = -1;
    float scale = 0.0f;
    Game* game = nullptr;
};

enum CartLabelType : uint8_t {
    CART_LABEL_NONE  = 0,
    CART_LABEL_SMALL = 1,
    CART_LARGE_LABEL = 2,
};

enum CartRenderFlags : uint8_t {
    CART_RENDER_DEFAULT        = 0,
    CART_RENDER_SLIM           = 1 << 0,
    CART_RENDER_LABEL_ONLY     = 1 << 1,
    CART_RENDER_PREFER_HIGHRES = 1 << 2
};

/**
 * Handles all cartridge rendering (both 2D orthographic and 3D perspective)
 */
class CartRenderer {
private:
    Size displaySize;

    /**
     * Front facing geometry of an N64 cartridge.
     * Slightly cheaper with vertices than the full model.
     */
    T3DModel* flatCartModel;
    
    /**
     * Full geometry of an N64 cartridge.
     * This is the version that can be freely rotated.
     */
    T3DModel* fullCartModel;
    
    // Geometry of just a 2x2 sprite label area of a cartridge
    T3DModel* texturedLabelModel;
    T3DModel* blankLabelModel;

    // Geometry of just a 4x4 sprite label area of a cartridge
    T3DModel* hdTexturedLabelModel;
    T3DModel* hdTexturedLabelCornerModel;

    LabelLoader& labelLoader;

    // Value-initialised: only indexes 0..14 get a placeholder in the
    // constructor, and [15] is filled in only by the large-label path.
    surface_t placeholders[DB_L_LABEL_TILE_COUNT] = {};

    surface_t labelSurfaces[LABEL_CACHE_MAX_S_ENTRIES];

    T3DMat4FP* cartListMatFP = nullptr;

    /**
     * Perspective viewport.
     * Used for rendering 3D depictions of the cartridge/label.
     */
    T3DViewport persViewport;

    /**
     * Orthographic viewport.
     * Used for rendering flat/2D depictions of the cartridge/label.
     */
    T3DViewport orthViewport;

    rspq_block_t* fullCartBlock;
    rspq_block_t* flatCartBlock;

    // Render only the textured label
    rspq_block_t* blankLabelBlock;
    rspq_block_t* fadedTexturedLabelBlock;
    rspq_block_t* texturedLabelBlock;
    rspq_block_t* highResTexturedLabelBlock;

    PendingCartPreload pendingCartPreload;

    void setupBlocks();

    Rect renderCart(int frameNumber, float scale, Vec3f position, Vec3f rotation, Game* game, float opacity, uint8_t flags);

public:
    CartRenderer(Size displaySize, LabelLoader& labelLoader);
    ~CartRenderer();

    int preloadLabelDataForGame(Game* game, bool preferHighRes, int tileCount = -1);
    CartLabelType loadLabelDataForGame(Game* game, bool preferHighRes = false);

    void fillSurfaces();

    /**
     * Get the size (in pixels) for a given scale
     */
    Size sizeForScale(float scale) const;

    void finishPreload();
    int prerender2DCart(float scale, Game* game);

    /**
     * Render a 2D orthographic cartridge
     */
    Rect render2DCart(int frameNumber, float scale, Vec2 screenPosition, float yRelativeToLight, Game* game, float opacity, int cacheIndex);

    /**
     * Render a 3D perspective cartridge
     */
    Rect render3DCart(int frameNumber, float scale, Vec3f position, Vec3f rotation, Vec2 screenPosition, Game* game, float intensity, uint8_t flags);

    void freeAllLabels();
};
