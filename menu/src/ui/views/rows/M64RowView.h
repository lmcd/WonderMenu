/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/CheckboxView.h"
#include "ui/views/drawables/ChevronView.h"
#include "ui/views/drawables/LabelReferenceView.h"

struct M64RowView : public BaseRowView {
    const char* name() const override { return "M64RowView"; }

    CheckboxView checkboxView;
    LabelReferenceView<128> titleView;
    LabelReferenceView<128> subtitleView;

    bool isChecked = false;

    M64RowView();

    void update(const RenderInfo& renderInfo) override;
};
