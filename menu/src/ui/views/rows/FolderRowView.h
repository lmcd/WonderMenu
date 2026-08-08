/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>

#include "ui/View.h"
#include "ui/views/TableView.h"
#include "ui/views/drawables/CheckboxView.h"
#include "ui/views/drawables/LabelView.h"

struct FolderEntry {
    std::string path;
    bool isSelected = false;
};

/**
 * A single row of ChooseFoldersPopover
 * A checkbox and the folder's name.
 */
struct FolderRowView : public BaseRowView {
    const char* name() const override { return "FolderRowView"; }

    CheckboxView checkboxView;
    LabelView<72> titleView;

    // Assigned by the scene every frame. Rows are a fixed pool that the table
    // recycles as it scrolls, so a row is never tied to one entry.
    FolderEntry* entry = nullptr;

    FolderRowView();

    void update(const RenderInfo& renderInfo) override;
};
