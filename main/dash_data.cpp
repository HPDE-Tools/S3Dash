#include "dash_data.h"

/**
 * Clamp the values from the given struct to sensible default. Note: this is not thread-safe,
 * so it should never be applied to the shared dash data struct.
 */
void DashData::clamp(dash_data_t *dash_data)
{
    dash_data->oil_pressure = std::clamp(dash_data->oil_pressure, 0, 160);
    dash_data->engine_coolant_temp = std::clamp(dash_data->engine_coolant_temp, 0, 300);
    dash_data->oil_temp = std::clamp(dash_data->oil_temp, 0, 300);
    dash_data->brake_per = std::clamp(dash_data->brake_per, 0, 100);
    dash_data->throttle_per = std::clamp(dash_data->throttle_per, 0, 100);
    dash_data->steering = std::clamp(dash_data->steering, -900, 900);
}
