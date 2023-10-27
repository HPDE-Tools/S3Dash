#ifndef S3DASH_DASH_DATA_H
#define S3DASH_DASH_DATA_H

#include <algorithm>

typedef struct {
    int rpm;
    int oil_pressure0;
    int oil_pressure1;
    int oil_temp;
    int engine_coolant_temp;
    int throttle_per;
    int brake_per;
    int steering;
} dash_data_t;

namespace DashData { 
    enum RpmLevel { NONE, ONE, TWO, THREE, FOUR, FIVE, SIX, SHIFT, OVERREV };

    void clamp(dash_data_t *dash_data);

    RpmLevel calcRpmLevel(int rpm);
}

uint16_t bitsToUIntLe(uint8_t *payload, uint8_t bitOffset, uint8_t bitLength);

#endif
