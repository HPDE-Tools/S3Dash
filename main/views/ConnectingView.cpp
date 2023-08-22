#include "ConnectingView.h"

ConnectingView::ConnectingView(Sprite *renderOn)
{
    sprite = renderOn;
}

void ConnectingView::render()
{
    sprite->setTextColor(Color::COLOR_WHITE);
    sprite->setTextColor(0xFFFFFF);
    sprite->setFont(&fonts::DejaVu18);
    sprite->setTextSize(1);
    sprite->drawCenterString("Connecting...", 
                             LCD_H_RES / 2, 
                             LCD_V_RES / 2 - 8 // optically center
                             );
}
