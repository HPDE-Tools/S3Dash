#include "dash_data.h"


/**
 * Clamp the values from the given struct to sensible default. Note: this is not thread-safe,
 * so it should never be applied to the shared dash data struct.
 */
void DashData::clamp(dash_data_t *dash_data)
{
    dash_data->rpm = std::clamp(dash_data->rpm, 0, 9999);
    dash_data->oil_pressure = std::clamp(dash_data->oil_pressure, 0, 160);
    dash_data->engine_coolant_temp = std::clamp(dash_data->engine_coolant_temp, 0, 300);
    dash_data->oil_temp = std::clamp(dash_data->oil_temp, 0, 300);
    dash_data->brake_per = std::clamp(dash_data->brake_per, 0, 100);
    dash_data->throttle_per = std::clamp(dash_data->throttle_per, 0, 100);
    dash_data->steering = std::clamp(dash_data->steering, -900, 900);
}

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

uint16_t bitsToUIntLe(uint8_t *payload, uint8_t bitOffset, uint8_t bitLength)
{
    uint16_t result = 0;
    uint8_t byteOffset = bitOffset / 8;
    uint8_t bitOffsetInByte = bitOffset % 8;
    uint8_t bitsLeft = bitLength;
    uint8_t bitsLeftInByte = 8 - bitOffsetInByte;
    uint8_t bitsToRead = bitsLeftInByte < bitsLeft ? bitsLeftInByte : bitsLeft;
    uint8_t mask = (1 << bitsToRead) - 1;
    result = (payload[byteOffset] >> bitOffsetInByte) & mask;
    bitsLeft -= bitsToRead;
    while (bitsLeft > 0)
    {
        byteOffset++;
        bitsToRead = bitsLeft < 8 ? bitsLeft : 8;
        mask = (1 << bitsToRead) - 1;
        result |= (payload[byteOffset] & mask) << (8 - bitOffsetInByte);
        bitOffsetInByte = 0;
        bitsLeft -= bitsToRead;
    }
    return result;
}
