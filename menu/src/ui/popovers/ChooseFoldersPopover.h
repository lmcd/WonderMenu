/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>
#include <vector>

#include "ui/Popover.h"
#include "ui/views/rows/FolderRowView.h"

/**
 * Popover for choosing several ROM folders at once.
 * Used at launch to pick which folders to scan.
 */
class ChooseFoldersPopover : public Popover<FolderRowView, std::vector<std::string>> {
private:
    InputWatcher aButtonWatcher;

    std::vector<FolderEntry> folderEntries;

    void toggleSelectedFolder();

    std::vector<std::string> selectedPaths() const;
    int selectedCount() const;

protected:
    std::vector<std::string> selectedValue() override { return selectedPaths(); }

public:
    const char* name() { return "ChooseFoldersPopover"; }

    ChooseFoldersPopover(Scene* baseScene, const std::vector<std::string>& paths);

    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo);
};
