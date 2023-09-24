#ifndef S3DASH_CONNECTING_VIEW_H
#define S3DASH_CONNECTING_VIEW_H

#include "color.h"
#include "lcd.h"
#include "sprite.h"

class ConnectingView 
{
private:
    Sprite *sprite;
public:
    ConnectingView(Sprite *renderOn);
    void render();
};

#endif
