
#pragma once

#include <helpers/ui/DisplayDriver.h>

namespace MenuUi {

static const uint8_t VISIBLE = 4;
static const int FIRST_Y = 13;
static const int ROW_H = 12;

inline void header(
    DisplayDriver& display,
    const char* title,
    const char* right,
    bool
) {
    display.setColor(UIColor::title_bkg);
    display.fillRect(0, 0, display.width(), 11);

    display.setTextSize(1);
    display.setColor(UIColor::title_txt);

    display.setCursor(2, 1);
    display.print(title);

    if (right && right[0]) {
        display.drawTextRightAlign(display.width() - 2, 1, right);
    }
}

inline void row(
    DisplayDriver& display,
    int y,
    const char* label,
    const char* value,
    bool selected,
    bool warn
) {
    const int width = display.width();

    if (selected) {
        display.setColor(UIColor::corp_blue);
        display.fillRect(0, y - 1, width, ROW_H);
    }

    display.setTextSize(1);

    if (warn) {
        display.setColor(UIColor::warning_txt);
    } else if (selected) {
        display.setColor(UIColor::title_txt);
    } else {
        display.setColor(UIColor::primary_txt);
    }

    const int value_width =
        (value && value[0]) ? display.getTextWidth(value) : 0;

    const int value_x = width - value_width - 3;
    const int label_width = value_width > 0
        ? value_x - 5
        : width - 5;

    display.drawTextEllipsized(3, y, label_width, label);

    if (value && value[0]) {
        display.setCursor(value_x, y);
        display.print(value);
    }
}

}
