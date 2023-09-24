#ifndef S3DASH_DASH_DATA_H
#define S3DASH_DASH_DATA_H

#include <algorithm>

typedef struct {
    int oil_pressure;
    int oil_temp;
    int engine_coolant_temp;
    int throttle_per;
    int brake_per;
    int steering;
} dash_data_t;

namespace DashData { 
    void clamp(dash_data_t *dash_data);
}

#endif
