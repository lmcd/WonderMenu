/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "LabelLoader.h"
#include "Game.h"

#include <algorithm>

static Game* currentLargeLabelGame;
static int currentLargeLabelTileCount = 0;

LabelLoader::LabelLoader(GameDatabase& database)
    : database(database) {
    for (int i = 0; i < LABEL_CACHE_MAX_S_ENTRIES; i++) {
        entries[i] = { nullptr, CartLabel::LABEL_FREE, i };
    }
}

LabelLoader::~LabelLoader() {}

int LabelLoader::firstAvailableIndex() {
    for (int i = 0; i < LABEL_CACHE_MAX_S_ENTRIES; i++) {
        if (entries[i].status == CartLabel::LABEL_FREE) {
            return i;
        }
    }

    for (int i = 0; i < LABEL_CACHE_MAX_S_ENTRIES; i++) {
        if (entries[i].status == CartLabel::LABEL_PURGABLE) {
            return i;
        }
    }

    return -1;
}

void LabelLoader::freeLabelForGame(Game* game) {
    if (game->cartLabel == nullptr) {
        return;
    }

    game->cartLabel->status = CartLabel::LABEL_PURGABLE;
}

void LabelLoader::freeAll() {
    for (int i = 0; i < LABEL_CACHE_MAX_S_ENTRIES; i++) {
        if (entries[i].status == CartLabel::LABEL_LOADED) {
            entries[i].status = CartLabel::LABEL_PURGABLE;
        }
    }
}

LoadResult LabelLoader::loadSmallLabelDataForGame(Game* game, int _tileCount, bool isPreloading) {
    assertf(_tileCount % DB_LABEL_TILES_PER_CHUNK == 0, "'tileCount' must be a multiple of %i", DB_LABEL_TILES_PER_CHUNK);
    assertf(_tileCount <= DB_S_LABEL_TILE_COUNT, "'tileCount' is greater than %i", DB_S_LABEL_TILE_COUNT);
    assertf(_tileCount != 0, "'tileCount' was 0");

    int tileCount = _tileCount;
    int tileOffset = 0;

    CartLabel* cartLabel = game->cartLabel;

    // Does the game already have an assigned label?
    if (cartLabel != nullptr) {
        bool gameMatches = (game == cartLabel->game);

        if (gameMatches) {
            switch (cartLabel->status) {
                case CartLabel::LABEL_FREE:
                    assertf(false, "Game shouldn't have a label marked LABEL_FREE");
                    break;
                
                case CartLabel::LABEL_PURGABLE:
                    // Label was still available but was marked for purge
                    // Take ownership of it again by marking t as loaded
                    cartLabel->status = CartLabel::LABEL_LOADED;
                    return LoadResult(0, cartLabel->data);

                case CartLabel::LABEL_LOADED:
                    // Label is already loaded and available, so just return it
                    return LoadResult(0, cartLabel->data);

                case CartLabel::LABEL_PARTIAL:
                    tileCount = DB_S_LABEL_TILE_COUNT - cartLabel->currentTileCount;
                    tileOffset = cartLabel->currentTileCount;

                    break;
            }
        }
        else {
            cartLabel->game->cartLabel = nullptr;
            cartLabel = nullptr;
        }
    }

    if (!isPreloading) {
        return LoadResult(0, nullptr);
    }

    int cacheIndex;

    if (cartLabel == nullptr) {
        cacheIndex = firstAvailableIndex();

        assertf(cacheIndex >= 0, "No available cache entry");

        cartLabel = &entries[cacheIndex];
        
        Game* game = cartLabel->game;

        if (game) {
            game->cartLabel = nullptr;
        }
    }
    else {
        cacheIndex = cartLabel->cacheIndex;
    }

    int bufferOffset = DB_S_LABEL_SIZE * cacheIndex;
    char* startBuffer = &smallLabelBuffer[bufferOffset];

    char* currentBuffer = &smallLabelBuffer[bufferOffset];

    int chunksToRead = tileCount / DB_LABEL_TILES_PER_CHUNK;

    for (int i = 0; i < chunksToRead; i++) {
        int tilesToRead = DB_LABEL_TILES_PER_CHUNK;
        
        bool result = database.loadCartLabelChunk(game->databaseEntry, false, currentBuffer, tileOffset, tilesToRead);

        if (!result) {
            startBuffer = nullptr;
            break;
        }

        tileOffset += DB_LABEL_TILES_PER_CHUNK;
    }

    cartLabel->game = game;
    cartLabel->cacheIndex = cacheIndex;
    cartLabel->data = startBuffer;

    if (tileOffset == DB_S_LABEL_TILE_COUNT) {
        cartLabel->status = CartLabel::LABEL_LOADED;
    }
    else {
        cartLabel->status = CartLabel::LABEL_PARTIAL;
    }

    cartLabel->currentTileCount = tileOffset;

    game->cartLabel = cartLabel;

    return LoadResult(tileCount, startBuffer);
}

LoadResult LabelLoader::loadLargeLabelDataForGame(Game* game, int _tileCount) {
    assertf(_tileCount % DB_LABEL_TILES_PER_CHUNK == 0, "'tileCount' must be a multiple of %i", DB_LABEL_TILES_PER_CHUNK);
    assertf(_tileCount <= DB_L_LABEL_TILE_COUNT, "'tileCount' is greater than %i", DB_L_LABEL_TILE_COUNT);
    assertf(_tileCount != 0, "'tileCount' was 0");
    
    int maximumTileCount = DB_L_LABEL_TILE_COUNT;

    if (game == currentLargeLabelGame && currentLargeLabelTileCount == maximumTileCount) {
        return LoadResult(0, (char*)largeLabelBuffer);
    }

    if (game != currentLargeLabelGame) {
        currentLargeLabelTileCount = 0;
    }

    int tileOffset = currentLargeLabelTileCount;

    // Picks up where a partial load left off, so only what is still missing is
    // read: asking for the full count on top of a partial load would otherwise
    // write past the end of `largeLabelBuffer`
    int tilesToRead = std::min(_tileCount, maximumTileCount - tileOffset);
    int chunksToRead = tilesToRead / DB_LABEL_TILES_PER_CHUNK;
    int loadedTileCount = 0;

    for (int i = 0; i < chunksToRead; i++) {
        int tileCount = DB_LABEL_TILES_PER_CHUNK;
        bool result = database.loadCartLabelChunk(game->databaseEntry, true, (char*)largeLabelBuffer, tileOffset, tileCount);
        
        if (!result) {
            return LoadResult(0, nullptr);
        }

        loadedTileCount += tileCount;
        tileOffset += tileCount;
    }

    currentLargeLabelTileCount += loadedTileCount;
    currentLargeLabelGame = game;

    return LoadResult(loadedTileCount, (char*)largeLabelBuffer);
}
