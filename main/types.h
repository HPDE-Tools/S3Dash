#ifndef S3DASH_TYPES_H
#define S3DASH_TYPES_H

typedef struct {
    int oil_pressure;
    int oil_temp;
    int engine_coolant_temp;
    int throttle_per;
    int brake_per;
    int steering;
} dash_data_t;

#endif
