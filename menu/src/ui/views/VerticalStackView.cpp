/**
* @copyright 2026 - Lee McDermott
* @license AGPLv3
*/

#include "VerticalStackView.h"

void VerticalStackView::update(const RenderInfo&) {
    int y = 0;
    bool isFirst = true;

    for (View* subview : subviews) {
        if (subview->isHidden) {
            continue;
        }

        if (!isFirst) {
            y += spacing;
        }

        isFirst = false;

        switch (alignment) {
            case LEADING:
                subview->frame.origin.x = 0;
                break;

            case CENTER:
                subview->frame.origin.x = (frame.size.width - subview->frame.size.width) / 2;
                break;

            case TRAILING:
                subview->frame.origin.x = frame.size.width - subview->frame.size.width;
                break;

            case FILL:
                subview->frame.origin.x = 0;
                subview->frame.size.width = frame.size.width;
                break;
        }

        subview->frame.origin.y = y;
        y += subview->frame.size.height;
    }

    frame.size.height = y;
}
