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
} MeasurementChr;

bool BLE_init(void);

void BLE_UpdateValue(MeasurementChr name, uint16_t value);

#endif  // BLE_H