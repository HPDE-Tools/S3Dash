#ifndef S3DASH_SPRITE_H
#define S3DASH_SPRITE_H

#include <LovyanGFX.h>
#include "color.h"

class Sprite: public LGFX_Sprite
{
public:
    inline void progressBarFromBottom(int x, int y, int width, int height, double percent, uint16_t color)
    {
        int computed_height = percent * height;
        this->fillRect(x, y, width, height, Color::COLOR_GRAY_DARK);
        this->fillRect(x, y + height - computed_height, width, computed_height, color);
    }

    inline void progressBarFromLeft(int x, int y, int width, int height, double percent, uint16_t color)
    {
        this->fillRect(x, y, width * percent, height, color);
    }

    inline void drawRightNumber(int value, int x, int y)
    {
        this->drawRightString(std::to_string(value).c_str(), x, y);
    }
};

#endif
