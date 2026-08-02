/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/CheckboxView.h"
#include "ui/views/drawables/LabelReferenceView.h"

struct CheatOptionRowView : public BaseRowView {
    const char* name() const override { return "CheatOptionRowView"; }

    CheckboxView checkboxView;
    LabelReferenceView<72> titleView;

    CheatOptionRowView();

    void update(const RenderInfo& renderInfo) override;
};
