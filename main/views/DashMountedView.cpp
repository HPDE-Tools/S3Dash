#include "DashMountedView.h"

#define UI_COLUMN_BEGIN_1 2
#define UI_COLUMN_BEGIN_2 236
#define UI_LABEL_HEIGHT 21
#define UI_ROW_HEIGHT 58
#define UI_ROW_BEGIN_1 (UI_SAFE_ZONE_MARGIN)
#define UI_ROW_BEGIN_2 (UI_SAFE_ZONE_MARGIN + UI_ROW_HEIGHT)
#define UI_ROW_BEGIN_3 (UI_SAFE_ZONE_MARGIN + UI_ROW_HEIGHT * 2)

DashMountedView::DashMountedView(Sprite *renderOn)
{
    sprite = renderOn;
}

void DashMountedView::setupText(UseCase useCase)
{
    switch (useCase) {
    case LABEL:
        sprite->setTextColor(Color::COLOR_GRAY_LIGHT);
        sprite->setFont(&fonts::DejaVu18);
        sprite->setTextSize(1);
        break;
    case VALUE_LARGE:
        sprite->setTextColor(Color::COLOR_WHITE);
        sprite->setFont(&fonts::Font7);
        sprite->setTextSize(2);
        break;
    case VALUE_SMALL:
        sprite->setTextColor(Color::COLOR_WHITE);
        sprite->setFont(&fonts::Font7);
        sprite->setTextSize(.55);
        break;
    }
}

void DashMountedView::render(dash_data_t *dash_data)
{
    sprite->fillScreen(0);
    sprite->setColor(Color::COLOR_WHITE);

    // OilP
    setupText(LABEL);
    sprite->drawString("OILP (PSI)", UI_COLUMN_BEGIN_1, UI_ROW_BEGIN_1);
    setupText(VALUE_LARGE);
    sprite->drawRightNumber(dash_data->oil_pressure, 220, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

    // OilT
    setupText(LABEL);
    sprite->drawString("OILT (F)", UI_COLUMN_BEGIN_2, UI_SAFE_ZONE_MARGIN);

    setupText(VALUE_SMALL);
    sprite->drawRightNumber(dash_data->oil_temp, LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_1 + UI_LABEL_HEIGHT);

    // ECT
    setupText(LABEL);
    sprite->drawString("ECT (F)", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_2);

    setupText(VALUE_SMALL);
    sprite->drawRightNumber(dash_data->engine_coolant_temp, LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_2 + UI_LABEL_HEIGHT);

    // PPS / Brake
    setupText(LABEL);
    sprite->drawString("THROTTLE /", UI_ROW_BEGIN_1, UI_ROW_BEGIN_3 + 4);
    sprite->setTextColor(Color::COLOR_RED);
    sprite->drawString("BRAKE", 128, UI_ROW_BEGIN_3 + 4);

    sprite->fillRect(0, 144, 220, 24, Color::COLOR_GRAY_DARK);
    sprite->progressBarFromLeft(0, 144, 220, 24, (double) dash_data->throttle_per / 100, Color::COLOR_WHITE);
    sprite->progressBarFromLeft(0, 144, 220, 24, (double) dash_data->brake_per / 100, Color::COLOR_RED);

    setupText(LABEL);
    sprite->drawString("STEER", UI_COLUMN_BEGIN_2, UI_ROW_BEGIN_3);

    // Steering
    setupText(VALUE_SMALL);
    sprite->drawRightNumber(dash_data->steering, LCD_H_RES - UI_SAFE_ZONE_MARGIN, UI_ROW_BEGIN_3 + UI_LABEL_HEIGHT);
}
