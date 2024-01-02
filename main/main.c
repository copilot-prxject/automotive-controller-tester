#include <stdio.h>

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "modules/cli.c"
#include "modules/adc.h"

#define BLINK_GPIO GPIO_NUM_2

static uint8_t s_led_state = 0;

static void blink_led(void) {
    gpio_set_level(BLINK_GPIO, s_led_state);
    s_led_state = !s_led_state;
}

static void configure_led(void) {
    gpio_reset_pin(BLINK_GPIO);

    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void) {
    // TODO: ADC range, PWM, cmd & BLE commands (PWM control, measurement control)
// Initialize ESP Console
    CLI_init();
    configure_led();
    ADC_init();

    while (1) {
        blink_led();

        // ESP_LOGI(__func__, "Value: %u mV", ADC_read());
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ADC_deinit();
}
