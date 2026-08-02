/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/FlagView.h"
#include "ui/views/drawables/LabelReferenceView.h"
#include "ui/views/drawables/LabelView.h"

class Game;

struct GameVersionRowView : public BaseRowView {
    const char* name() const override { return "GameVersionRowView"; }

    FlagView flagView;
    LabelReferenceView<16> titleView;
    LabelView<6> subtitleView;

    Game* game = nullptr;

    GameVersionRowView();

    void setTitle(const char* utf8_fmt, ...);

    void update(const RenderInfo& renderInfo) override;
};
