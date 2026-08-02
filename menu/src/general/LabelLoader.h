/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <list>
#include <unordered_map>
#include <cstdint>

#include "GameDatabase.h"
#include "Game.h"
#include "util/Color.h"
#include "util/Rect.h"

#define LABEL_CACHE_MAX_S_ENTRIES 24
#define LABEL_CACHE_MAX_L_ENTRIES 1

class Game;

struct LoadResult {
    int tilesLoaded = 0;
    char* data;
};

struct CartLabel {
    enum Size {
        LABEL_SIZE_SMALL,
        LABEL_SIZE_LARGE
    };

    enum Status {
        LABEL_FREE,
        LABEL_PARTIAL,
        LABEL_LOADED,
        LABEL_PURGABLE
    };

    char* data;
    Status status;
    int cacheIndex;
    int currentChunkCount;
    Game* game;
};

class LabelLoader {
private:
    GameDatabase& database;

    CartLabel entries[LABEL_CACHE_MAX_S_ENTRIES];

    alignas(8) char smallLabelBuffer[DB_S_LABEL_SIZE * LABEL_CACHE_MAX_S_ENTRIES];
    alignas(8) char largeLabelBuffer[DB_L_LABEL_SIZE * LABEL_CACHE_MAX_L_ENTRIES];

public:
    LabelLoader(GameDatabase& database);
    ~LabelLoader();

    void freeLabelForGame(Game* game);
    void freeAll();
    int firstAvailableIndex();

    LoadResult getSmallLabelDataForGame(Game* game, int tileCount, bool isPreloading = false);
    LoadResult getLargeLabelDataForGame(Game* game, bool loadIncrementally = false);
};
