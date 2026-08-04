/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include <cfloat>

#include "CartRenderer.h"

#define MATRIX_COUNT 10

int matIndexes[BUFF_COUNT] = {0};

Rect getModelScreenBoundingBox(T3DViewport& viewport, const T3DModel* model, const T3DMat4* modelMatrix) {
    float minX = model->aabbMin[0];
    float minY = model->aabbMin[1];
    float minZ = model->aabbMin[2];

    float maxX = model->aabbMax[0];
    float maxY = model->aabbMax[1];
    float maxZ = model->aabbMax[2];

    T3DVec3 corners[8] = {
        {{minX, minY, minZ}},
        {{maxX, minY, minZ}},
        {{minX, maxY, minZ}},
        {{maxX, maxY, minZ}},
        {{minX, minY, maxZ}},
        {{maxX, minY, maxZ}},
        {{minX, maxY, maxZ}},
        {{maxX, maxY, maxZ}},
    };

    float bMinX = +FLT_MAX;
    float bMinY = +FLT_MAX;
    float bMaxX = -FLT_MAX;
    float bMaxY = -FLT_MAX;

    for (int i = 0; i < 8; i++) {
        T3DVec4 worldPos4;
        t3d_mat4_mul_vec3(&worldPos4, modelMatrix, &corners[i]);
        T3DVec3 worldPos = {{worldPos4.v[0], worldPos4.v[1], worldPos4.v[2]}};

        T3DVec3 screenPos;
        t3d_viewport_calc_viewspace_pos(&viewport, &screenPos, &worldPos);

        if (screenPos.v[0] < bMinX) bMinX = screenPos.v[0];
        if (screenPos.v[1] < bMinY) bMinY = screenPos.v[1];
        if (screenPos.v[0] > bMaxX) bMaxX = screenPos.v[0];
        if (screenPos.v[1] > bMaxY) bMaxY = screenPos.v[1];
    }

    return Rect(
        (int)bMinX,
        (int)bMinY,
        (int)(bMaxX - bMinX),
        (int)(bMaxY - bMinY)
    );
}

static void dynamicTextureCallback(void* userData, const T3DMaterial* material, rdpq_texparms_t* tileParams, rdpq_tile_t tile) {
    if(tile != TILE0) {
        return;
    }

    surface_t* placeholders = (surface_t*)userData;

    int texReference = material->textureA.texReference;
    int placeholderIndex = texReference - 1;

    rdpq_sync_tile();

    rdpq_tex_upload(TILE0, &placeholders[placeholderIndex], NULL);
}

CartRenderer::CartRenderer(Size displaySize, LabelLoader& labelLoader)
    : displaySize(displaySize)
    , labelLoader(labelLoader) {

    for (int i = 0; i < 15; i++) {
        placeholders[i] = surface_make_placeholder_linear(i + 1, FMT_RGBA16, DB_LABEL_TILE_WIDTH, DB_LABEL_TILE_HEIGHT);
    }

    cartListMatFP = (T3DMat4FP*)malloc_uncached(sizeof(T3DMat4FP) * BUFF_COUNT * MATRIX_COUNT);

    flatCartModel = t3d_model_load("rom:/models/CartridgeFlatNew1.t3dm");
    fullCartModel = t3d_model_load("rom:/models/CartridgeFullNew1.t3dm");
    texturedLabelModel = t3d_model_load("rom:/models/CartridgeLabelsTextured1.t3dm");
    blankLabelModel = t3d_model_load("rom:/models/CartridgeLabelsFlat1.t3dm");
    hdTexturedLabelModel = t3d_model_load("rom:/models/CartridgeLabelsTextured4x41.t3dm");
    hdTexturedLabelCornerModel = t3d_model_load("rom:/models/CartridgeLabelsTextured4x4Corner1.t3dm");

    setupBlocks();

    T3DVec3 camPosition = {{0.0f, 0.0f, 0.0f}};
    T3DVec3 camTarget = {{0.0f, 0.0f, -DISTANCE_FROM_CAMERA}};
    T3DVec3 cameraUp = {{0, 1, 0}};

    float nearPlane = 1.0f;
    float farPlane = 150.0f;
    float fov = T3D_DEG_TO_RAD(50.0f);

    persViewport = t3d_viewport_create_buffered(BUFF_COUNT);
    orthViewport = t3d_viewport_create_buffered(BUFF_COUNT);

    // Setup perspective viewport - used for 3D cartridge render
    t3d_viewport_set_projection(&persViewport, fov, nearPlane, farPlane);
    // Setup orthographic viewport - used for 2D cartridge render
    t3d_viewport_set_ortho(&orthViewport, -30, 30, -22.5, 22.5, nearPlane, farPlane);

    t3d_viewport_look_at(&persViewport, &camPosition, &camTarget, &cameraUp);
    t3d_viewport_look_at(&orthViewport, &camPosition, &camTarget, &cameraUp);

    Size labelSize(32, 36);

    for (int i = 0; i < LABEL_CACHE_MAX_S_ENTRIES; i++) {
        labelSurfaces[i] = surface_alloc(FMT_RGBA16, labelSize.width, labelSize.height);
    }
}

CartRenderer::~CartRenderer() {
    if (cartListMatFP) {
        free_uncached(cartListMatFP);
        cartListMatFP = nullptr;
    }
}

void CartRenderer::fillSurfaces() {
    for (int i = 0; i < LABEL_CACHE_MAX_S_ENTRIES; i++) {
        // Fill the surface red via the RDP
        rdpq_attach(&labelSurfaces[i], NULL);
        // TODO: Can we use the RGBA16 transparency bit?
        rdpq_clear(Color(141));
        rdpq_detach();
    }
}

void CartRenderer::setupBlocks() {  
    blankLabelBlock = rspq_make_block([&] {
        t3d_model_draw(blankLabelModel);
    });

    fadedTexturedLabelBlock = rspq_make_block([&] {
        T3DModelDrawConf conf = (T3DModelDrawConf){
            .userData = &placeholders,
            .dynTextureCb = dynamicTextureCallback,
        };
        T3DModelState state = t3d_model_state_create();
        state.drawConf = &conf;

        T3DModelIter it = t3d_model_iter_create(texturedLabelModel, T3D_CHUNK_TYPE_OBJECT);
        
        while(t3d_model_iter_next(&it))
        {
            if(conf.filterCb && !conf.filterCb(conf.userData, it.object)) {
                continue;
            }

            if(it.object->material) {
                // auto oldColorFlags = it.object->material->setColorFlags;
                // it.object->material->setColorFlags = 0x000;

                t3d_model_draw_material(it.object->material, &state);

                // rdpq_mode_begin();
                //  rdpq_mode_combiner(RDPQ_COMBINER1((SHADE, 0, TEX0, 0), (0, 0, 0, PRIM)));
                //  rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
                //  rdpq_mode_antialias(AA_NONE);
                // rdpq_mode_end();
                rdpq_mode_blender(0);

                // it.object->material->setColorFlags = oldColorFlags;

                // rdpq_mode_antialias(AA_NONE);
            }
            
            t3d_model_draw_object(it.object, conf.matrices);
        }
    });

    texturedLabelBlock = rspq_make_block([&] {
        t3d_model_draw_custom(texturedLabelModel, (T3DModelDrawConf){
            .userData = &placeholders,
            .dynTextureCb = dynamicTextureCallback,
        });
    });

    highResTexturedLabelBlock = rspq_make_block([&] {
        t3d_model_draw_custom(hdTexturedLabelModel, (T3DModelDrawConf){
            .userData = &placeholders,
            .dynTextureCb = dynamicTextureCallback,
        });
    });

    flatCartBlock = rspq_make_block([&] {
        // t3d_model_draw(flatCartModel);
        t3d_model_draw(fullCartModel);
    });

    fullCartBlock = rspq_make_block([&] {
        t3d_model_draw(fullCartModel);
    });
}

Rect CartRenderer::renderCart(int frameNumber, float scale, Vec3f position, Vec3f rotation, Game* game, float opacity, uint8_t flags) {
    rdpq_mode_push();
    
    rdpq_mode_begin();
        rdpq_set_mode_standard();
        rdpq_mode_antialias(AA_STANDARD);
        // rdpq_mode_zbuf(true, true);
        rdpq_mode_persp(true);

        rdpq_mode_filter(FILTER_BILINEAR);
        rdpq_mode_dithering(DITHER_SQUARE_SQUARE);

        //rdpq_mode_blender(0);
        rdpq_mode_fog(0);
    rdpq_mode_end();
    
    bool preferHighRes = (flags & CART_RENDER_PREFER_HIGHRES);
    CartLabelType labelType = loadLabelDataForGame(game, preferHighRes);

    int bufferIndex = (frameNumber % BUFF_COUNT);
    matIndexes[bufferIndex] = (matIndexes[bufferIndex] + 1) % MATRIX_COUNT;

    int i = bufferIndex + (matIndexes[bufferIndex] * BUFF_COUNT);

    T3DMat4 modelMat;

    t3d_mat4_from_srt_euler(
        &modelMat,
        (float[3]){scale, scale, scale},
        (float[3]){rotation.x, rotation.y, rotation.z},
        (float[3]){position.x, position.y, position.z}
    );

    t3d_mat4_to_fixed(&cartListMatFP[i], &modelMat);
    t3d_matrix_push(&cartListMatFP[i]);
    
    rspq_block_t* labelBlock;

    if (labelType == CART_LARGE_LABEL) {
        labelBlock = highResTexturedLabelBlock;
    }
    else {
        // if (opacity < 1.0f) {
            labelBlock = fadedTexturedLabelBlock;
        // }
        // else {
            // labelBlock = texturedLabelBlock;
        // }
    }

    if (flags & CART_RENDER_LABEL_ONLY) {
        if (labelType) {
            // if (opacity < 1.0f) {
            //  rdpq_set_blend_color((color_t){0, 0, 0, 255});
            //  rdpq_set_prim_color((color_t){255, 255, 255, (uint8_t)(opacity * 255)});
            // }

            rspq_block_run(labelBlock);
        }
    } else {
        if (flags & CART_RENDER_SLIM) {
            rspq_block_run(flatCartBlock);
        } else {
            rspq_block_run(fullCartBlock);
        }

        if (!labelType) {
            rspq_block_run(blankLabelBlock);
        }
        else {
            rspq_block_run(labelBlock);
        }
    }

    if (labelType == CART_LARGE_LABEL) {
        t3d_model_draw_custom(hdTexturedLabelCornerModel, (T3DModelDrawConf){
            .userData = &placeholders,
            .dynTextureCb = dynamicTextureCallback
        });
    }

    t3d_matrix_pop(1);

    rdpq_sync_pipe();

    rdpq_mode_pop();

    return getModelScreenBoundingBox(persViewport, fullCartModel, &modelMat);
}

int CartRenderer::preloadLabelDataForGame(Game* game, bool preferHighRes, int tileCount) {
    if (!game) {
        return 0;
    }

    LoadResult result;

    if (preferHighRes) {
        result = labelLoader.getLargeLabelDataForGame(game, true);
    }
    else {
        result = labelLoader.getSmallLabelDataForGame(game, tileCount, true);
    }

    return result.tilesLoaded;
}

CartLabelType CartRenderer::loadLabelDataForGame(Game* game, bool preferHighRes) {
    char* spriteData = nullptr;

    if (game) {
        if (preferHighRes) {
            LoadResult result = labelLoader.getLargeLabelDataForGame(game, false);
            spriteData = result.data;

            if (spriteData) {
                for (int i = 0; i < DB_L_LABEL_TILE_COUNT; i++) {
                    void* pointer = spriteData + (DB_LABEL_TILE_SIZE * i);

                    if (i == 15) {
                        placeholders[i] = surface_make((char*)pointer, FMT_RGBA16, DB_LABEL_TILE_WIDTH, DB_LABEL_TILE_HEIGHT, TEX_FORMAT_PIX2BYTES(FMT_RGBA16, DB_LABEL_TILE_WIDTH));
                    }
                    else {
                        rdpq_set_lookup_address(i + 1, pointer);
                    }
                }

                return CART_LARGE_LABEL;
            }
        }

        if (spriteData == nullptr) {
            LoadResult result = labelLoader.getSmallLabelDataForGame(game, 4);
            spriteData = result.data;

            if (spriteData) {
                for (int i = 0; i < DB_S_LABEL_TILE_COUNT; i++) {
                    rdpq_set_lookup_address(i + 1, spriteData + (DB_LABEL_TILE_SIZE * i));
                }

                return CART_LABEL_SMALL;
            }
        }
    }

    return CART_LABEL_NONE;
}

Size CartRenderer::sizeForScale(float scale) const {
    return (CART_MODEL_SIZE * scale).even();
}

void CartRenderer::finishPreload() {
    // debugf("dma_busy %i\n", dma_busy());
    if (pendingCartPreload.cacheIndex == -1) {
        // Nothing to preload
        return;
    }

    int cacheIndex = pendingCartPreload.cacheIndex;
    Game* game = pendingCartPreload.game;
    float scale = pendingCartPreload.scale;

    Size size = sizeForScale(scale);

    Vec2 sizeMidpoint = size.mid();

    Size labelSize(32, 36);

    sizeMidpoint.x -= (size.width  - labelSize.width)  / 2;
    sizeMidpoint.y -= (size.height - labelSize.height) / 2;

    rdpq_attach(&labelSurfaces[cacheIndex], nullptr);

    render2DCart(
        0,
        scale,
        sizeMidpoint,
        0,
        game,
        1.0f,
        -1
    );

    rdpq_detach();

    pendingCartPreload.cacheIndex = -1;
}

int CartRenderer::prerender2DCart(float scale, Game* game) {
    int tilesLoaded = preloadLabelDataForGame(game, false, 4);

    finishPreload();

    if (tilesLoaded > 0) {
        pendingCartPreload = PendingCartPreload(game->cartLabel->cacheIndex, scale, game);
    }

    return tilesLoaded;
}

Rect CartRenderer::render2DCart(int frameNumber, float scale, Vec2 screenPosition, float yRelativeToLight, Game* game, float opacity = 1.0f, int cacheIndex = -1) {
    Size size = sizeForScale(scale);

    if (cacheIndex >= 0) {
        if (cacheIndex == pendingCartPreload.cacheIndex) {
            dma_wait();
            finishPreload();
        }

        uint8_t fade = (uint8_t)(opacity * 255);
        Color color = Color(fade);

        Vec2 position = screenPosition - size.mid();

        surface_t* labelSurface = &labelSurfaces[cacheIndex];

        Size labelSize(
            labelSurface->width,
            labelSurface->height
        );
        
        position.x += (size.width  - labelSize.width)  / 2;
        position.y += (size.height - labelSize.height) / 2;

        rdpq_mode_push();
        
        rdpq_mode_begin();
            rdpq_set_mode_standard();
            rdpq_mode_blender(0);
            rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, PRIM, 0), (0, 0, 0, ONE)));
        rdpq_mode_end();

        rdpq_set_prim_color(color);

        rdpq_sync_tile();

        rdpq_tex_blit(labelSurface, position.x, position.y, NULL);

        rdpq_mode_pop();
    }
    else {
        Vec2 offsetToZero = -displaySize.mid();

        t3d_viewport_set_area(&orthViewport, offsetToZero.x + screenPosition.x, offsetToZero.y + screenPosition.y, displaySize.width, displaySize.height);
        t3d_viewport_attach2(&orthViewport);

        float lightIntensity = 40.0f * opacity;
        T3DVec3 pointLightPos = {{0.0f, -1.1f + yRelativeToLight, -32.0f}};

        t3d_light_set_count(1);
        t3d_light_set_point(0, (uint8_t[]){0xFC, 0xFC, 0xFF, 0xFF}, &pointLightPos, lightIntensity, false);

        uint8_t flags = CART_RENDER_SLIM;

        if (true) {
            // Size size = sizeForScale(scale);
            // Vec2 sizeMidpoint = size.mid();

            // rdpq_mode_push();

            // if (opacity == 1.0f) {
            //  rdpq_set_mode_copy(false);
            // }
            // else {
            //  rdpq_mode_begin();
            //      rdpq_set_mode_standard();
            //      rdpq_mode_combiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, PRIM)));
            //      rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, BLEND_RGB, INV_MUX_ALPHA)));
            //  rdpq_mode_end();

            //  rdpq_set_prim_color((color_t){255, 255, 255, (uint8_t)(opacity * 255)});
            // }

            // rdpq_sync_tile();

            // rdpq_sprite_blit(cartSprite, screenPosition.x - sizeMidpoint.x, screenPosition.y - sizeMidpoint.y, NULL);

            // rdpq_mode_pop();

            flags |= CART_RENDER_LABEL_ONLY;
        }

        renderCart(
            frameNumber,
            scale,
            Vec3f(0.0f, 0.0f, -DISTANCE_FROM_CAMERA),
            Vec3f(),
            game,
            opacity,
            flags
        );
    }

    Rect boundingBox = {screenPosition, size};
    boundingBox.origin.x -= size.midX();
    boundingBox.origin.y -= size.midY();

    return boundingBox;
}

Rect CartRenderer::render3DCart(int frameNumber, float scale, Vec3f position, Vec3f rotation, Vec2 screenPosition, Game* game, float intensity, uint8_t flags) {
    Vec2 offsetToZero = -displaySize.mid();

    t3d_viewport_set_area(&persViewport, offsetToZero.x + screenPosition.x, offsetToZero.y + screenPosition.y, displaySize.width, displaySize.height);
    t3d_viewport_attach2(&persViewport);

    float lightIntensity = 40.0f * intensity;
    T3DVec3 pointLightPos = {{0.0f, -5.1f, -32.0f}};

    t3d_light_set_count(1);
    t3d_light_set_point(0, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF}, &pointLightPos, lightIntensity, false);

    return renderCart(
        frameNumber,
        scale,
        position,
        rotation,
        game,
        1.0f,
        flags
    );
}

void CartRenderer::freeAllLabels() {
    labelLoader.freeAll();
}
