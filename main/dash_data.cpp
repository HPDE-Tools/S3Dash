#include "dash_data.h"


DashData::RpmLevel DashData::calcRpmLevel(int rpm)
{
    if (rpm < 4700) return NONE;
    if (rpm < 5100) return ONE;
    if (rpm < 5500) return TWO; 
    if (rpm < 5900) return THREE;
    if (rpm < 6300) return FOUR;
    if (rpm < 6800) return FIVE;
    if (rpm < 7200) return SIX;
    if (rpm < 7525) return SHIFT;
    return OVERREV;
}

