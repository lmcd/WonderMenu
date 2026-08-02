/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/Scene.h"
#include "ui/views/drawables/ScrollbarView.h"

// Base class for the scenes shown inside GameInfoScene's tab container.
class GameInfoTabScene : public Scene {
public:
    ScrollbarView* scrollbarView = nullptr;

    int yRenderOffset = 0;
};
