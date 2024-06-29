#include "modules/ct.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modules/cli.h"
#include "modules/ble.h"

// Define constants
#define ADC_MAX_VALUE 4095

typedef struct {
    Millivolt min;
    Millivolt max;
    Millivolt avg;
} AdcMeas;

typedef struct {
    Amper min;
    Amper max;
    Amper avg;
} CurrentMeas;

static struct {
    Amper max_current;
    unsigned ratio;
    float step;
    float base;

    bool interupt_measurements;
    bool calibrated;

    bool ongoing;
    Seconds duration;

    adc_oneshot_unit_handle_t adc2_handle;
    adc_cali_handle_t  adc2_cali_handle;
    adc_channel_t default_channel;
    adc_channel_t ref_channel;
    adc_atten_t default_atten;
    adc_bitwidth_t default_width;
} ctx = {
    .max_current        = 50,
    .ratio              = 4,
    // .step               = 12.5,
    .step               = 0.0125,
    .default_channel    = ADC_CHANNEL_8,
    .ref_channel        = ADC_CHANNEL_7,
    .default_atten      = ADC_ATTEN_DB_11,
    .default_width      = ADC_BITWIDTH_DEFAULT
};


static int ct_command_execution(int argc, char **argv) {
    if (argc == 1){
        ESP_LOGI(__func__, "No arguments");
    }

    static const char now[] = "now";
    static const char duration[] = "duration";
    for (int i = 1; i < argc; i++) {
        ESP_LOGI(__func__, "Arg %d: %s", i, argv[i]);
        if (strncmp(now, argv[i], sizeof(now)) == 0) {
            ESP_LOGI(__func__, "CT: %f A", CT_read());
        }
        if (strncmp(duration, argv[i], sizeof(now)) == 0) {
            if (argc > i + 1) {
                CT_read_for(atoi(argv[i + 1]));
            }

            return 0;
        }
    }
    return 0;
}

static void parse_ble_command(char *buffer, unsigned length) {
    CT_read_for(strtoul(buffer, NULL, 0));
}

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(__func__`, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(__func__, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ctx.default_width,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(__func__, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(__func__, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(__func__, "Invalid arg or no memory");
    }

    return calibrated;
}

void CT_init(void) {
    // Initialize ADC configuration here
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_2,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &ctx.adc2_handle));

    //-------------ADC1 Config---------------//
    const adc_oneshot_chan_cfg_t config = {
        .bitwidth = ctx.default_width,
        .atten = ctx.default_atten,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ctx.adc2_handle, ctx.default_channel, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ctx.adc2_handle, ctx.ref_channel, &config));

    //-------------ADC1 Calibration Init---------------//
    ctx.calibrated = adc_calibration_init(ADC_UNIT_2, ctx.default_channel, ctx.default_atten, &ctx.adc2_cali_handle);
    ctx.calibrated = ctx.calibrated && adc_calibration_init(ADC_UNIT_2, ctx.ref_channel, ctx.default_atten, &ctx.adc2_cali_handle);

    CLI_register_command("ct", "[now] [duration <time>]", ct_command_execution);
    BLE_setup_characteristic_callback(kCurrent, parse_ble_command);
}

Amper CT_read(void) {
    // Initialize variables
    Amper meas;
    unsigned sample = 0;
    unsigned samples = 100;
    unsigned step = 10;

    int voltage = 0;
    int ref_voltage = 0;
    float sum = 0;
    float ref_sum = 0;

    while (sample < samples) {
        // Read ADC value
        int adc_value;
        int adc_ref_value;
        ESP_ERROR_CHECK(adc_oneshot_read(ctx.adc2_handle, ctx.default_channel, &adc_value));
        ESP_ERROR_CHECK(adc_oneshot_read(ctx.adc2_handle, ctx.ref_channel, &adc_ref_value));

        if (ctx.calibrated) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(ctx.adc2_cali_handle, adc_value, &voltage));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(ctx.adc2_cali_handle, adc_ref_value, &ref_voltage));
        }

        sum += voltage;
        ref_sum += ref_voltage;
        sample += 1;
        vTaskDelay(pdMS_TO_TICKS(step));
    }

    // calc avg ref
    ref_sum = ((ref_sum / 1000) * 2 / samples);  // - ctx.base;
    // calc avg V
    meas = (sum / 1000) / samples;

    ESP_LOGI(__func__, "CT V [in: %f, ref: %f]",  meas, ref_sum);

    // diff, base -> offset
    meas = meas - ref_sum;
    // take into account ratio and step
    meas = meas / (ctx.ratio * ctx.step);

    // ct duration 1000

    // Calculate average
    ESP_LOGI(__func__, "CT C [Avg %f round: %f]", meas, round(meas));
    if (meas < 0.0f)
        meas *= -1.0f; 

    return meas;
}

static void read_ct_for() {
    CurrentMeas meas;
    // Initialize variables
    unsigned time = 0;
    unsigned endTime = ctx.duration * 1000;
    unsigned step = 1000;

    Amper amp = 0;
    Amper sum = 0;
    meas.min = ctx.max_current;
    meas.max = 0;
    meas.avg = 0;
    char buffer[11 + 11 + 11 + 11];
    // Perform measurements for the specified duration
    while (time < endTime) {
        memset(buffer, 0, sizeof(buffer));
        // Read ADC value
        amp = CT_read();
        // Update sum, min, and max
        sum += amp;
        if (amp < meas.min)
            meas.min = amp;
        
        if (amp > meas.max)
            meas.max = amp;

        time += step;

        meas.avg = sum / (time / step);
        // ESP_LOGI(__func__, "CT: [now: %f A] [max %f A] [min %f A]", amp, meas.max, meas.min);
        snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f,%.2f", amp, meas.max, meas.min, meas.avg);
        BLE_update_value(kCurrent, buffer);
        vTaskDelay(pdMS_TO_TICKS(step));
    }

    ESP_LOGI(__func__, "CT: [avg %.2f A] [max %.2f A] [min %.2f A]", meas.avg, meas.max, meas.min);
    vTaskDelete(NULL);
}

void CT_read_for(Seconds duration) {
    ctx.ongoing = true;
    ctx.duration = duration;
    xTaskCreate(read_ct_for, "current_read_for", 4096, NULL, 5, NULL);
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGD(__func__, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGD(__func__, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

void CT_deinit(void) {
    ESP_ERROR_CHECK(adc_oneshot_del_unit(ctx.adc2_handle));
    if (ctx.calibrated) {
        example_adc_calibration_deinit(ctx.adc2_cali_handle);
    }
}