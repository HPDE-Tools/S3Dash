#include "lib.h"

/**
 * Clamp the values from the given struct to sensible default. Note: this is not thread-safe,
 * so it should never be applied to the shared dash data struct.
 */
void clamp_dash_data(dash_data_t *d)
{
    d->oil_pressure = std::clamp(d->oil_pressure, 0, 160);
    d->engine_coolant_temp = std::clamp(d->engine_coolant_temp, 0, 300);
    d->oil_temp = std::clamp(d->oil_temp, 0, 300);
    d->brake_per = std::clamp(d->brake_per, 0, 100);
    d->throttle_per = std::clamp(d->throttle_per, 0, 100);
    d->steering = std::clamp(d->steering, -900, 900);
}

int transform(int a_input, int a_low, int a_high, int b_low, int b_high)
{ 
    return a_input * (b_high - b_low) / (a_high - a_low);
}
