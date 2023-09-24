#include "SteeringWheelMountedView.h"

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

void SteeringWheelMountedView::MetricView(int x, int y, int width, metric_t *metric)
{
    LabelView(metric->label, x, y);

    sprite->setTextColor(Color::COLOR_WHITE);
    sprite->setFont(&fonts::Font7);
    sprite->setTextSize(.5);

    sprite->drawRightNumber(metric->value, x + width, y + 16);
}

void SteeringWheelMountedView::HeroMetricView(int x, int y, int width, metric_t *metric)
{
    LabelView(metric->label, x, y);

    sprite->setTextColor(Color::COLOR_WHITE);
    sprite->setFont(&fonts::Font7);
    sprite->setTextSize(2.2);

    sprite->drawRightNumber(metric->value, x + width, y + 16);
}

void SteeringWheelMountedView::render(dash_data_t *dash_data)
{
    sprite->fillScreen(0);
    sprite->setColor(Color::COLOR_WHITE);

    sprite->progressBarFromBottom(UI_SAFE_ZONE_MARGIN, 
                                  UI_SAFE_ZONE_MARGIN, 
                                  20, 
                                  140, 
                                  (double) dash_data->brake_per / 100, 
                                  Color::COLOR_RED
                                  );
    sprite->progressBarFromBottom(UI_SAFE_ZONE_MARGIN + 20 + 2, 
                                  UI_SAFE_ZONE_MARGIN, 
                                  20, 
                                  140, 
                                  (double) dash_data->throttle_per / 100, 
                                  Color::COLOR_WHITE
                                  );
    LabelView("BRAKE / PPS", 56, 134);

    metric_t metric;
    metric.label = "OILP (PSI)";
    metric.value = dash_data->oil_pressure;
    HeroMetricView(56, UI_SAFE_ZONE_MARGIN, 180, &metric);

    int y = UI_SAFE_ZONE_MARGIN;
    const int METRIC_START = 248;
    const int METRIC_HEIGHT = 48;
    const int METRIC_WIDTH = 64;

    metric.label = "OILT (F)";
    metric.value = dash_data->oil_temp;
    MetricView(METRIC_START, y, METRIC_WIDTH, &metric);

    metric.label = "ECT (F)";
    metric.value = dash_data->engine_coolant_temp;
    MetricView(METRIC_START, y + METRIC_HEIGHT, METRIC_WIDTH, &metric);

    metric.label = "STEER";
    metric.value = dash_data->steering;
    MetricView(METRIC_START, y + METRIC_HEIGHT * 2, METRIC_WIDTH, &metric);
}
