#include <stdio.h>

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "modules/cli.c"
#include "modules/adc.h"
#include "modules/pwm.h"
#include "modules/relay.h"
#include "modules/ble.h"


void app_main(void) {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    CLI_init();
    ADC_init();

    if (RELAY_init() == false)
        ESP_LOGE("Starting", "Relay not initilized");

    if (PWM_init() == false)
        ESP_LOGE("Starting", "PWM not initilized");

    if (BLE_init() == false)
        ESP_LOGE("Starting", "Ble not initilized");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ADC_deinit();
}
