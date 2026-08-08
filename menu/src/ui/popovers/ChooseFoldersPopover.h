/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#pragma once

#include <string>
#include <vector>

#include "ui/Popover.h"
#include "ui/views/drawables/ImageView.h"
#include "ui/views/drawables/LabelView.h"
#include "ui/views/rows/FolderRowView.h"

/**
 * Popover for choosing several ROM folders at once.
 * Used at launch to pick which folders to scan.
 */
class ChooseFoldersPopover : public Popover<FolderRowView, std::vector<std::string>> {
private:
    using Base = Popover<FolderRowView, std::vector<std::string>>;

    InputWatcher aButtonWatcher;

    std::vector<FolderEntry> folderEntries;
    sprite_t* sdCardSprite = nullptr;

    void toggleSelectedFolder();

protected:
    std::vector<std::string> selectedValue() override;

public:
    const char* name() { return "ChooseFoldersPopover"; }

    ImageView imageView;
    LabelView<64> messageLabel;

    ChooseFoldersPopover(Scene* baseScene, const std::vector<std::string>& paths);

    void didBeginScene(SceneEntry entry) override;
    void update(const UpdateInfo& updateInfo) override;
    void updateViews(const RenderInfo& renderInfo);
};
