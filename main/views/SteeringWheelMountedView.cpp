#include "SteeringWheelMountedView.h"

#define TOP_SPACING 2
#define RPM_INDICATOR_LIGHT_COUNT 6
#define RPM_INDICATOR_LIGHT_LAST_INDEX (RPM_INDICATOR_LIGHT_COUNT - 1)
#define RPM_INDICATOR_GUTTER 5
#define RPM_INDICATOR_HEIGHT (LCD_V_RES - RPM_INDICATOR_LIGHT_COUNT * (RPM_INDICATOR_GUTTER - 1)) / RPM_INDICATOR_LIGHT_COUNT
#define RPM_INDICATOR_WIDTH 10

uint16_t color_warn_above_threshold(int value, int warning_threshold, int critical_threshold)
{
    if (value > critical_threshold)
    {
        return Color::COLOR_RED;
    } 
    if (value > warning_threshold)
    {
        return Color::COLOR_YELLOW;
    }
    return Color::COLOR_WHITE;
}

uint16_t color_warn_below_threshold(int value, int warning_threshold, int critical_threshold)
{
    if (value < critical_threshold)
    {
        return Color::COLOR_RED;
    } 
    if (value < warning_threshold)
    {
        return Color::COLOR_YELLOW;
    }
    return Color::COLOR_WHITE;
}

SteeringWheelMountedView::SteeringWheelMountedView(Sprite *renderOn)
{
    sprite = renderOn;
}

void SteeringWheelMountedView::LabelView(const char *value, int x, int y)
{
    sprite->setTextColor(Color::COLOR_GRAY_LIGHT);
    sprite->setFont(&fonts::DejaVu12);
    sprite->setTextSize(1);

    sprite->drawString(value, x, y);
}

void SteeringWheelMountedView::MetricView(int x, int y, int width, metric_t *metric, uint16_t color)
{
    LabelView(metric->label, x, y);

    sprite->setTextColor(color);
    sprite->setFont(&fonts::Font7);
    sprite->setTextSize(.5);

    sprite->drawRightNumber(metric->value, x + width, y + 16);
}

void SteeringWheelMountedView::ShiftRect(int index, uint16_t color)
{
    sprite->fillRect(LCD_H_RES - RPM_INDICATOR_WIDTH, 
                     (RPM_INDICATOR_LIGHT_LAST_INDEX - index) * RPM_INDICATOR_HEIGHT + (RPM_INDICATOR_LIGHT_LAST_INDEX - index) * RPM_INDICATOR_GUTTER, 
                     RPM_INDICATOR_WIDTH, 
                     RPM_INDICATOR_HEIGHT,
                     color);
}

void SteeringWheelMountedView::ShiftIndicator(dash_data_t *dash_data)
{
    DashData::RpmLevel rpmLevel = DashData::calcRpmLevel(dash_data->rpm);
    switch (rpmLevel)
    {
    case DashData::NONE:
        break;
    case DashData::ONE:
        ShiftRect(0, Color::COLOR_YELLOW);
        break;
    case DashData::TWO:
        ShiftRect(0, Color::COLOR_YELLOW);
        ShiftRect(1, Color::COLOR_YELLOW);
        break;
    case DashData::THREE:
        ShiftRect(0, Color::COLOR_YELLOW);
        ShiftRect(1, Color::COLOR_YELLOW);
        ShiftRect(2, Color::COLOR_ORANGE);
        break;
    case DashData::FOUR:
        ShiftRect(0, Color::COLOR_YELLOW);
        ShiftRect(1, Color::COLOR_YELLOW);
        ShiftRect(2, Color::COLOR_ORANGE);
        ShiftRect(3, Color::COLOR_ORANGE);
        break;
    case DashData::FIVE:
        ShiftRect(0, Color::COLOR_YELLOW);
        ShiftRect(1, Color::COLOR_YELLOW);
        ShiftRect(2, Color::COLOR_ORANGE);
        ShiftRect(3, Color::COLOR_ORANGE);
        ShiftRect(4, Color::COLOR_BLUE);        
        break;
    case DashData::SIX:
        ShiftRect(0, Color::COLOR_YELLOW);
        ShiftRect(1, Color::COLOR_YELLOW);
        ShiftRect(2, Color::COLOR_ORANGE);
        ShiftRect(3, Color::COLOR_ORANGE);
        ShiftRect(4, Color::COLOR_BLUE);
        ShiftRect(5, Color::COLOR_BLUE);
        break;
    case DashData::SHIFT:
        ShiftRect(0, Color::COLOR_BLUE);
        ShiftRect(1, Color::COLOR_BLUE);
        ShiftRect(2, Color::COLOR_BLUE);
        ShiftRect(3, Color::COLOR_BLUE);
        ShiftRect(4, Color::COLOR_BLUE);
        ShiftRect(5, Color::COLOR_BLUE);
        break;
    case DashData::OVERREV:
        ShiftRect(0, Color::COLOR_RED);
        ShiftRect(1, Color::COLOR_RED);
        ShiftRect(2, Color::COLOR_RED);
        ShiftRect(3, Color::COLOR_RED);
        ShiftRect(4, Color::COLOR_RED);
        ShiftRect(5, Color::COLOR_RED);
        break;
    }
}

void SteeringWheelMountedView::HeroMetricView(int x, int y, int width, metric_t *metric, uint16_t color)
{
    LabelView(metric->label, x, y);

    sprite->setTextColor(color);
    sprite->setFont(&fonts::Font7);
    sprite->setTextSize(2);

    sprite->drawRightNumber(metric->value, x + width, y + 16);
}

void SteeringWheelMountedView::render(dash_data_t *dash_data)
{
    sprite->fillScreen(0);
    
    sprite->setColor(Color::COLOR_WHITE);

    sprite->progressBarFromBottom(UI_SAFE_ZONE_MARGIN, 
                                  TOP_SPACING, 
                                  20, 
                                  140, 
                                  (double) dash_data->brake_per / 100, 
                                  Color::COLOR_RED
                                  );
    sprite->progressBarFromBottom(UI_SAFE_ZONE_MARGIN + 20 + 2, 
                                  TOP_SPACING, 
                                  20, 
                                  140, 
                                  (double) dash_data->throttle_per / 100, 
                                  Color::COLOR_WHITE
                                  );
    LabelView("BRAKE / PPS", 56, 130);

    metric_t metric;
    metric.label = "OILP (PSI)";
    metric.value = dash_data->oil_pressure;
    
    HeroMetricView(56, UI_SAFE_ZONE_MARGIN, 172, &metric, color_warn_below_threshold(metric.value, 30, 10));

    int y = UI_SAFE_ZONE_MARGIN;
    const int METRIC_START = 238;
    const int METRIC_HEIGHT = 48;
    const int METRIC_WIDTH = 64;

    ShiftIndicator(dash_data);

    metric.label = "OILT (F)";
    metric.value = dash_data->oil_temp;
    MetricView(METRIC_START, y, METRIC_WIDTH, &metric, color_warn_above_threshold(metric.value, 240, 260));

    metric.label = "ECT (F)";
    metric.value = dash_data->engine_coolant_temp;
    MetricView(METRIC_START, y + METRIC_HEIGHT, METRIC_WIDTH, &metric, color_warn_above_threshold(metric.value, 240, 260));

    metric.label = "STEER";
    metric.value = dash_data->steering;
    MetricView(METRIC_START, y + METRIC_HEIGHT * 2, METRIC_WIDTH, &metric, Color::COLOR_WHITE);
}
