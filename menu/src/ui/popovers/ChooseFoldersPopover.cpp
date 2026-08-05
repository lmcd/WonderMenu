/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ChooseFoldersPopover.h"

ChooseFoldersPopover::ChooseFoldersPopover(Scene* baseScene, const std::vector<std::string>& paths)
    : Popover<FolderRowView, std::vector<std::string>>(baseScene) {
    title = "CHOOSE FOLDERS";
    isCancellable = false;

    folderEntries.reserve(paths.size());

    for (const std::string& path : paths) {
        folderEntries.push_back(FolderEntry{ path, false });
    }
}

void ChooseFoldersPopover::toggleSelectedFolder() {
    int index = tableView.selectedRowIndex;

    if (index < 0 || index >= (int)folderEntries.size()) {
        return;
    }

    folderEntries[index].isSelected = !folderEntries[index].isSelected;
}

std::vector<std::string> ChooseFoldersPopover::selectedPaths() const {
    std::vector<std::string> paths;

    for (const FolderEntry& folder : folderEntries) {
        if (folder.isSelected) {
            paths.push_back(folder.path);
        }
    }

    return paths;
}

int ChooseFoldersPopover::selectedCount() const {
    int count = 0;

    for (const FolderEntry& folder : folderEntries) {
        if (folder.isSelected) {
            count++;
        }
    }

    return count;
}

void ChooseFoldersPopover::update(const UpdateInfo& updateInfo) {
    // A ticks the row rather than confirming, so Popover::update() is
    // deliberately not called here -- its A handler would close the popover on
    // the first tick.
    if (tableView.handleInputs(updateInfo)) {
        return;
    }

    if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_UP) {
        toggleSelectedFolder();
    }
}

void ChooseFoldersPopover::updateViews(const RenderInfo& renderInfo) {
    tableView.rowCount = (int)folderEntries.size();

    Popover<FolderRowView, std::vector<std::string>>::updateViews(renderInfo);

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        FolderRowView* rowView = &tableView.rowViews[i++];

        rowView->entry = &folderEntries[rowIndex];
    }
}
