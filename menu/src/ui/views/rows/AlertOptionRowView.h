/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/LabelReferenceView.h"

/**
 * A single alert choice: one centred title and nothing else.
 *
 * Like CheatOptionRowView without the checkbox -- the option is picked by
 * confirming the row, so there is no per-row state to show.
 */
struct AlertOptionRowView : public BaseRowView {
    const char* name() const override { return "AlertOptionRowView"; }

    LabelReferenceView<72> titleView;

    bool isDestructive = false;

    AlertOptionRowView();

    void update(const RenderInfo& renderInfo) override;
};
