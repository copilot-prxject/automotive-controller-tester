#include "modules/ds_sensor.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#include "ds18b20.h"
#include "owb.h"
#include "owb_rmt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modules/cli.h"
#include "modules/ble.h"

#define GPIO_DS18B20_0 GPIO_NUM_26 // (CONFIG_ONE_WIRE_GPIO)
#define MAX_DEVICES (8)
#define DS18B20_RESOLUTION (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD (1000)  // milliseconds


static struct {
    owb_rmt_driver_info rmt_driver_info;
    OneWireBus *owb;
    DS18B20_Info *devices[MAX_DEVICES];
    OneWireBus_ROMCode device_rom_codes[MAX_DEVICES];
    unsigned num_devices;

    bool ongoing;
    Seconds duration;
} ctx = { 0 };

static int ds_sensor_command_execution(int argc, char **argv) {
    if (argc == 1){
        ESP_LOGI(__func__, "No arguments");
    }

    static const char now[] = "now";
    static const char duration[] = "duration";
    for (int i = 1; i < argc; i++) {
        ESP_LOGI(__func__, "Arg %d: %s", i, argv[i]);
        if (strncmp(now, argv[i], sizeof(now)) == 0) {
            Temperatures temp = DS_SENSOR_read();
            ESP_LOGI(__func__, "DS SENSOR: first: %f [C], second: %f [C]", temp.first_sensor, temp.second_sensor);
        }
        if (strncmp(duration, argv[i], sizeof(now)) == 0) {
            if (argc > i + 1) {
                DS_SENSOR_read_for(atoi(argv[i + 1]));
            }

            return 0;
        }
    }
    return 0;
}

static void parse_ble_command(char *buffer, unsigned length) {
    DS_SENSOR_read_for(strtoul(buffer, NULL, 0));
}

void DS_SENSOR_init(void) {
    // Create a 1-Wire bus, using the RMT timeslot driver
    ctx.owb = owb_rmt_initialize(&ctx.rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(ctx.owb, true);  // enable CRC check for ROM code

    // Stable readings require a brief period before communication
    vTaskDelay(2000.0 / portTICK_PERIOD_MS);

    // Find all connected devices
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(ctx.owb, &search_state, &found);
    while (found) {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        ESP_LOGI(__func__, "Temperature sensor %d : %s\n", ctx.num_devices, rom_code_s);
        ctx.device_rom_codes[ctx.num_devices] = search_state.rom_code;
        ++ctx.num_devices;
        owb_search_next(ctx.owb, &search_state, &found);
    }

    ESP_LOGI(__func__, "Found %d device%s", ctx.num_devices, ctx.num_devices == 1 ? "" : "s");
    for (unsigned i = 0; i < ctx.num_devices; ++i) {
        ctx.devices[i] = ds18b20_malloc();  // heap allocation
        if (ctx.devices[i] == NULL) {
            ctx.num_devices = 0;
            ESP_LOGE(__func__, "Failed to allocate memory for ds18b20_info");
            return;
        }

        if (ctx.num_devices == 1) {
            ESP_LOGW(__func__, "Single device optimisations enabled");
            ds18b20_init_solo(ctx.devices[i], ctx.owb);  // only one device on bus
        } else {
            ds18b20_init(ctx.devices[i], ctx.owb, ctx.device_rom_codes[i]);  // associate with bus and device
        }
        ds18b20_use_crc(ctx.devices[i], true);  // enable CRC check on all reads
        ds18b20_set_resolution(ctx.devices[i], DS18B20_RESOLUTION);
    }

    // Check for parasitic-powered devices
    bool parasitic_power = false;
    ds18b20_check_for_parasite_power(ctx.owb, &parasitic_power);
    if (parasitic_power) {
        ESP_LOGW(__func__, "Parasitic-powered devices detected");
    }

    // In parasitic-power mode, devices cannot indicate when conversions are complete,
    // so waiting for a temperature conversion must be done by waiting a prescribed duration
    owb_use_parasitic_power(ctx.owb, parasitic_power);

    CLI_register_command("ds", "[now] [duration <time>]", ds_sensor_command_execution);
    BLE_setup_characteristic_callback(kTemperature, parse_ble_command);
}

Temperatures DS_SENSOR_read(void) {
    Temperatures temp = {0};
    if (ctx.num_devices == 0 || ctx.owb == NULL) {
        ESP_LOGE(__func__, "No DS18B20 devices detected or no OWB!\n");
        return temp;
    }

    if (ctx.num_devices > 0) {
        // Read temperatures more efficiently by starting conversions on all devices at the same time
        ds18b20_convert_all(ctx.owb);

        // All devices use the same resolution, so the first can determine the delay
        ds18b20_wait_for_conversion(ctx.devices[0]);

        // Read the results immediately after conversion otherwise it may fail
        // (using printf before reading may take too long)
        float readings[MAX_DEVICES] = {0};
        DS18B20_ERROR errors[MAX_DEVICES] = {0};
        for (int i = 0; i < ctx.num_devices; ++i) {
            errors[i] = ds18b20_read_temp(ctx.devices[i], &readings[i]);
        }

        // Print results in a separate loop, after all have been read
        // ESP_LOGI(__func__, "\nTemperature readings (degrees C):");
        for (int i = 0; i < ctx.num_devices; ++i) {
            if (errors[i] != DS18B20_OK)
                printf("\nTemperature readings error: %d\n", errors[i]);
            ESP_LOGI(__func__, "Sensor %d: %.1f", i, readings[i]);
        }
        temp.first_sensor = readings[0];
        temp.second_sensor = readings[1];
    }
    return temp;
}

static void read_for() {
    // Initialize variables
    unsigned time = 0;
    unsigned endTime = ctx.duration * 1000;
    unsigned step = 1000;

    char buffer[11 + 11];
    Temperatures temp = {0};
    // Perform measurements for the specified duration
    while (time < endTime) {
        memset(buffer, 0, sizeof(buffer));
        // Read ADC value
        temp = DS_SENSOR_read();

        time += step;
        snprintf(buffer, sizeof(buffer), "%f,%f", temp.first_sensor, temp.second_sensor);
        BLE_update_value(kTemperature, buffer);
        vTaskDelay(pdMS_TO_TICKS(step));
    }

    vTaskDelete(NULL);
}

void DS_SENSOR_read_for(Seconds duration) {
    ctx.ongoing = true;
    ctx.duration = duration;
    xTaskCreate(read_for, "ds_read_for", 4096, NULL, 5, NULL);
}

void DS_SENSOR_deinit(void) {
    for (int i = 0; i < ctx.num_devices; ++i) {
        ds18b20_free(&ctx.devices[i]);
    }
    owb_uninitialize(ctx.owb);
}