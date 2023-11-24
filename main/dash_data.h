#ifndef S3DASH_DASH_DATA_H
#define S3DASH_DASH_DATA_H

#include <algorithm>
#include <atomic>

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

typedef struct {
    std::atomic<int> rpm;
    std::atomic<int> oil_pressure0;
    std::atomic<int> oil_pressure1;
    std::atomic<int> oil_temp;
    std::atomic<int> engine_coolant_temp;
    std::atomic<int> throttle_per;
    std::atomic<int> brake_per;
    std::atomic<int> steering;
} dash_data_atomic_t;

namespace DashData { 
    enum RpmLevel { NONE, ONE, TWO, THREE, FOUR, FIVE, SIX, SHIFT, OVERREV };

    /**
     * Clamp the values from the given struct to sensible default. Note: this is not thread-safe,
     * so it should never be applied to the shared dash data struct.
     */
    inline void clamp(dash_data_t *dash_data)
    {
        dash_data->rpm = std::clamp(dash_data->rpm, 0, 9999);
        dash_data->oil_pressure0 = std::clamp(dash_data->oil_pressure0, 0, 200);
        dash_data->oil_pressure1 = std::clamp(dash_data->oil_pressure1, 0, 200);
        dash_data->engine_coolant_temp = std::clamp(dash_data->engine_coolant_temp, 0, 300);
        dash_data->oil_temp = std::clamp(dash_data->oil_temp, 0, 300);
        dash_data->brake_per = std::clamp(dash_data->brake_per, 0, 100);
        dash_data->throttle_per = std::clamp(dash_data->throttle_per, 0, 100);
        dash_data->steering = std::clamp(dash_data->steering, -900, 900);
    }

    inline void dash_data_copy(const dash_data_atomic_t &src, dash_data_t &dst) {
        dst.rpm = src.rpm;
        dst.oil_pressure0 = src.oil_pressure0;
        dst.oil_pressure1 = src.oil_pressure1;
        dst.oil_temp = src.oil_temp;
        dst.engine_coolant_temp = src.engine_coolant_temp;
        dst.throttle_per = src.throttle_per;
        dst.brake_per = src.brake_per;
        dst.steering = src.steering;
    }

    RpmLevel calcRpmLevel(int rpm);
}

inline uint32_t bitsToUIntLe(uint8_t *payload, uint32_t bitOffset, uint32_t bitLength) {
    uint32_t result = 0;
    if (bitOffset >= 64 || bitLength >32 || bitOffset + bitLength > 64)
        return 0;
    uint32_t startByte = bitOffset/8;
    uint32_t endByte = (bitOffset+bitLength - 1) / 8;
    uint8_t mask = 0;
    int shift = 0;
    for (int i = startByte; i <= endByte; i++ ) {
        if (i == startByte) {
            mask = (1U << (8 - bitOffset % 8)) - 1;
            result |= (payload[i] >> (bitOffset % 8)) & mask;
            shift += (8 - bitOffset % 8);
            continue;
        }
        if (i == endByte) {
            uint8_t bitsToRead = (bitOffset + bitLength) % 8;
            if (bitsToRead == 0) {
                result |= static_cast<uint32_t>(payload[i])<<shift;
            } else {
                mask = (1U << bitsToRead) - 1;
                result |= static_cast<uint32_t>(payload[i] & mask) <<shift;
            }
            continue;
        }
        result |= static_cast<uint32_t>(payload[i])<<shift;
        shift += 8;
    }
    return result;
}

#endif
