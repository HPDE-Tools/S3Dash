#ifndef S3DASH_DASH_MOUNTED_VIEW_H
#define S3DASH_DASH_MOUNTED_VIEW_H

#include "color.h"
#include "dash_data.h"
#include "DisplayModeView.h"
#include "lcd.h"
#include "sprite.h"

enum OilPressureMode {OILP_0, OILP_1};
enum DisplayMode { DASH_MOUNT, STEERING_WHEEL_MOUNT };

class DashMountedView: public DisplayModeView 
{
private:
    Sprite *sprite;

    OilPressureMode oilPMode;

    enum UseCase { LABEL, VALUE_LARGE, VALUE_SMALL };

    void setupText(UseCase useCase);

public: 
    DashMountedView(Sprite *renderOn);

    void render(dash_data_t *dash_data);

    void setOilP(OilPressureMode mode);
};

#endif
