/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "ChooseFoldersPopover.h"

ChooseFoldersPopover::ChooseFoldersPopover(Scene* baseScene, const std::vector<std::string>& paths)
    : Popover<FolderRowView, std::vector<std::string>>(baseScene) {
    title = "CHOOSE FOLDERS";
    isCancellable = false;

    sdCardSprite = sprite_load("rom:/ui/SD-128.RGBA16.sprite");

    imageView.sprite = sdCardSprite;

    messageLabel.fontID = Fonts::INTERDISPLAY_BOLD_14;
    messageLabel.align = ALIGN_CENTER;
    messageLabel.textColor = Color(140);
    messageLabel.setString("%i $07ROM folders found!\nWhich would you like to use?", paths.size());

    folderEntries.reserve(paths.size());

    for (const std::string& path : paths) {
        folderEntries.push_back(FolderEntry{ path, true });
    }
}

void ChooseFoldersPopover::didBeginScene(SceneEntry entry) {
    Base::didBeginScene(entry);

    contentView.addSubview(&imageView);
    contentView.addSubview(&messageLabel);
}

void ChooseFoldersPopover::toggleSelectedFolder() {
    int index = tableView.selectedRowIndex;

    folderEntries[index].isSelected ^= true;
}

std::vector<std::string> ChooseFoldersPopover::selectedValue() {
    std::vector<std::string> selectedPaths;

    for (const FolderEntry& folder : folderEntries) {
        if (folder.isSelected) {
            selectedPaths.push_back(folder.path);
        }
    }

    return selectedPaths;
}

void ChooseFoldersPopover::update(const UpdateInfo& updateInfo) {
    tableView.handleInputs(updateInfo);

    if (updateInfo.btn.start) {
        hasMadeSelection = true;
    }
    else if (aButtonWatcher.update(updateInfo.joypad.btn.a) == InputWatcher::BUTTON_DOWN) {
        toggleSelectedFolder();
    }
}

void ChooseFoldersPopover::updateViews(const RenderInfo& renderInfo) {
    tableView.rowCount = (int)folderEntries.size();

    Base::updateViews(renderInfo);
    titleLabelView.isHidden = true;
    tableY = 134;

    Size imageSize = Size(sdCardSprite->width, sdCardSprite->height);

    imageView.frame = Rect(
        Vec2(
            rectView.frame.midX() - imageSize.midX(),
            rectView.frame.minY() + 30
        ),
        imageSize
    );

    messageLabel.frame.origin = Vec2(
        rectView.frame.minX(),
        imageView.frame.maxY() + 26
    );
    messageLabel.maxWidth = rectView.frame.size.width;

    auto indexRange = tableView.visibleIndexRange();
    int i = 0;

    for (int rowIndex : indexRange) {
        FolderRowView* rowView = &tableView.rowViews[i++];

        rowView->entry = &folderEntries[rowIndex];
    }
}
