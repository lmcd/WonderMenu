/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ChooseVersionPopover.h"

#include "general/Game.h"

ChooseVersionPopover::ChooseVersionPopover(Scene* baseScene, GameGroup* gameGroup)
    : Popover<GameVersionRowView, Game*>(baseScene),
      gameGroup(gameGroup) {
    title = "CHOOSE VERSION";
}

Game* ChooseVersionPopover::selectedValue() {
    return gameGroup->sortedGames()[tableView.selectedRowIndex];
}

void ChooseVersionPopover::updateViews(const RenderInfo& renderInfo) {
    tableView.rowCount = gameGroup->sortedGames().size();

    Popover<GameVersionRowView, Game*>::updateViews(renderInfo);

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        Game* game = gameGroup->sortedGames()[rowIndex];

        GameVersionRowView* rowView = &tableView.rowViews[i++];

        rowView->game = game;
    }
}
