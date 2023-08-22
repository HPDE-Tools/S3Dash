#ifndef S3DASH_DASH_MOUNTED_VIEW_H
#define S3DASH_DASH_MOUNTED_VIEW_H

#include "color.h"
#include "dash_data.h"
#include "DisplayModeView.h"
#include "lcd.h"
#include "sprite.h"

class DashMountedView: public DisplayModeView 
{
private:
    Sprite *sprite;

    enum UseCase { LABEL, VALUE_LARGE, VALUE_SMALL };

    void setupText(UseCase useCase);

public: 
    DashMountedView(Sprite *renderOn);

    void render(dash_data_t *dash_data);
};

#endif
