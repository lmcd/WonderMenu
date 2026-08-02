/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "LabelLoader.h"

static Game* currentLargeLabelGame;
static int currentLargeLabelChunkCount = 0;

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
            // debugf("Reusing purgable\n");
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

LoadResult LabelLoader::getSmallLabelDataForGame(Game* game, int _tileCount, bool isPreloading) {
    assertf(_tileCount % DB_LABEL_TILES_PER_CHUNK == 0, "'tileCount' must be a multiple of %i", DB_LABEL_TILES_PER_CHUNK);
    assertf(_tileCount <= DB_S_LABEL_TILE_COUNT, "'tileCount' is greater than %i", DB_S_LABEL_TILE_COUNT);
    assertf(_tileCount != 0, "'tileCount' was 0");

    int maximumTileCount = std::min(_tileCount, DB_S_LABEL_TILE_COUNT);

    int tileCount = maximumTileCount;
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
                    tileCount = DB_S_LABEL_TILE_COUNT - cartLabel->currentChunkCount;
                    tileOffset = cartLabel->currentChunkCount;

                    // debugf("Found partial CartLabel\n");
                    // debugf("tileCount: %i\n", tileCount);
                    // debugf("tileOffset: %i\n", tileOffset);

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

    // bufferOffset += (tileOffset / DB_LABEL_TILES_PER_CHUNK) * DB_LABEL_CHUNK_SIZE;

    char* currentBuffer = &smallLabelBuffer[bufferOffset];

    int chunksToRead = tileCount / DB_LABEL_TILES_PER_CHUNK;

    // debugf("-cacheIndex %i\n", cacheIndex);
    // debugf("-chunksToRead %i\n", chunksToRead);
    // debugf("-bufferOffset %i\n", bufferOffset);

    for (int i = 0; i < chunksToRead; i++) {
        int tilesToRead = DB_LABEL_TILES_PER_CHUNK;
        
        bool result = database.loadCartLabelChunk(game->databaseEntry, false, currentBuffer, tileOffset, tilesToRead);

        if (!result) {
            startBuffer = nullptr;
            break;
        }

        // debugf("---chunk %i\n", i);
        // debugf("---bufferOffset %i\n", bufferOffset);
        // debugf("---tileOffset %i\n", tileOffset);
        // debugf("---tilesToRead %i\n", tilesToRead);

        // debugf("------------------\n");

        tileOffset += DB_LABEL_TILES_PER_CHUNK;

        if (tileOffset == maximumTileCount) {
            break;
        }
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

    cartLabel->currentChunkCount = tileOffset;

    game->cartLabel = cartLabel;

    return LoadResult(tileCount, startBuffer);
}

LoadResult LabelLoader::getLargeLabelDataForGame(Game* game, bool loadIncrementally) {
    int maximumChunkCount = DB_L_LABEL_TILE_COUNT;

    if (game == currentLargeLabelGame && currentLargeLabelChunkCount == maximumChunkCount) {
        return LoadResult(0, (char*)largeLabelBuffer);
    }

    if (game != currentLargeLabelGame) {
        currentLargeLabelChunkCount = 0;
    }

    int chunkCount;
    int chunkOffset;

    if (loadIncrementally) {
        chunkCount = DB_LABEL_TILES_PER_CHUNK;
        chunkOffset = currentLargeLabelChunkCount;
    }
    else {
        chunkCount = maximumChunkCount - currentLargeLabelChunkCount;
        chunkOffset = currentLargeLabelChunkCount;
    }

    currentLargeLabelChunkCount += chunkCount;

    bool result = database.loadCartLabelChunk(game->databaseEntry, true, (char*)largeLabelBuffer, chunkOffset, chunkCount);
    
    if (!result) {
        return LoadResult(0, nullptr);
    }

    currentLargeLabelGame = game;

    return LoadResult(chunkCount, (char*)largeLabelBuffer);
}
