#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    kVoltage = 0,
    kCurrent,
    kTemperature,
// sentinel
    kLastMeasurementChr,

    kPWM,
    kRelay,
// sentinel
    kLastChr
} Characteristic;

typedef void (*CharacteristicCallback)(char *buffer, unsigned length);

bool BLE_init(void);

void BLE_setup_characteristic_callback(Characteristic name, CharacteristicCallback callback);
void BLE_update_value(Characteristic name, char *buffer);

#endif  // BLE_H