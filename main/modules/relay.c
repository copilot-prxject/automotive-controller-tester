#include "modules/relay.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modules/base/generic_fun.h"
#include "modules/cli.h"
#include "modules/ble.h"


static struct {
    bool last_state;
} ctx = {
    .last_state     = 0
};

static bool FoundArgument(int argc, char **argv, const char *arg) {
    for (int i = 1; i < argc; i++) {
        ESP_LOGI("arg", "arg: %s %s %s", argv[i], arg, AreStringsTheSame(arg, argv[i], strlen(arg)) ? "same" : "not");
        if (AreStringsTheSame(arg, argv[i], strlen(arg)) == false)
            continue;

        return true;
    }
    return false;
}

static const char state_on[] = "on";
static const char state_off[] = "off";
static int relay_command_execution(int argc, char **argv) {
    if (argc == 1){
        ESP_LOGI(__func__, "No arguments");
        return 0;
    }

    if (FoundArgument(argc, argv, state_on)){
        RELAY_set_state(true);
        ctx.last_state = true;
    }

    if (FoundArgument(argc, argv, state_off)){
        RELAY_set_state(false);
        ctx.last_state = false;
    }

    ESP_LOGI(__func__, "RELAY: %s", ctx.last_state ? "on" : "off");
    return 0;
}

static void parse_ble_command(char *buffer, unsigned length) {
    if (length == 0)
        return;

    if (AreStringsTheSame(state_off, buffer, strlen(state_off)) == true) {
        RELAY_set_state(false);
        ctx.last_state = false;
    }

    if (AreStringsTheSame(state_on, buffer, strlen(state_on)) == true) {
        RELAY_set_state(true);
        ctx.last_state = true;
    }

    unsigned value = strtoul(buffer, buffer + length, 0);
    RELAY_set_state(value);
    ctx.last_state = value;
    ESP_LOGI(__func__, "RELAY: %s", ctx.last_state ? "on" : "off");
}

bool RELAY_set_state(bool state) {
    esp_err_t err = gpio_set_level(GPIO_NUM_14, state);
    if (err != ESP_OK) {
        ESP_LOGW(__func__, "Problem with pin: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

// #define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
// #define GPIO_OUTPUT_IO_1    CONFIG_GPIO_OUTPUT_1
// #define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

bool RELAY_init(void) {
    gpio_config_t io_conf = { 0 };

    io_conf.pin_bit_mask    = (1ULL << GPIO_NUM_14),
    io_conf.intr_type       = GPIO_INTR_DISABLE;
    io_conf.mode            = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en    = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en      = GPIO_PULLUP_DISABLE;
    
    esp_err_t err = gpio_config(&io_conf);

    if (err != ESP_OK) {
        ESP_LOGW(__func__, "Problem with initialization: %s", esp_err_to_name(err));
        return false;
    }


    CLI_register_command("relay", "[on] [off]", relay_command_execution);
    BLE_setup_characteristic_callback(kRelay, parse_ble_command);
    RELAY_set_state(false);
    return true;
}

