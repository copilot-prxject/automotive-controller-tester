#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// bool BLE_init(char* data);
// static void gatts_profile_OBD_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
typedef enum {
    kVoltage = 0,
    kCurrent,
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