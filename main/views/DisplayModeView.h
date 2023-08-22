#ifndef S3DASH_DISPLAY_MODE_VIEW_H
#define S3DASH_DISPLAY_MODE_VIEW_H

#include "dash_data.h"

class DisplayModeView 
{
public:
    virtual void render(dash_data_t *dash_data) = 0;
};

#endif
