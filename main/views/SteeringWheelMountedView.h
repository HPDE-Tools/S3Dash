#ifndef S3DASH_STEERING_WHEEL_MOUNTED_VIEW_H
#define S3DASH_STEERING_WHEEL_MOUNTED_VIEW_H

#include "color.h"
#include "dash_data.h"
#include "DisplayModeView.h"
#include "lcd.h"
#include "metric.h"
#include "sprite.h"

class SteeringWheelMountedView: public DisplayModeView 
{
private:
    Sprite *sprite;

    void LabelView(const char *value, int x, int y);
    void MetricView(int x, int y, int width, metric_t *metric);
    void HeroMetricView(int x, int y, int width, metric_t *metric);
    void ShiftIndicator(dash_data_t *dash_data);
    void ShiftRect(int index, uint16_t color);

public: 
    SteeringWheelMountedView(Sprite *renderOn);

    void render(dash_data_t *dash_data);
};

#endif
