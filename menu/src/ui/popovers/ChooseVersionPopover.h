/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/Popover.h"
#include "ui/views/rows/GameVersionRowView.h"

class Game;
struct GameGroup;

/**
 * Popover for choosing a regional veriant of a game.
 */
class ChooseVersionPopover : public Popover<GameVersionRowView, Game*> {
private:
    GameGroup* gameGroup;

protected:
    Game* selectedValue() override;

public:
    const char* name() { return "ChooseVersionPopover"; }

    ChooseVersionPopover(Scene* baseScene, GameGroup* gameGroup);

    void updateViews(const RenderInfo& renderInfo);
};
