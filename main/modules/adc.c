#include "modules/adc.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static struct {
    bool interupt_measurements;
    bool calibrated;

    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t  adc1_cali_handle;
    adc_channel_t default_channel;
    adc_atten_t default_atten;
    adc_bitwidth_t default_width;
} ctx = {
    .default_channel = ADC_CHANNEL_6,
    .default_atten = ADC_ATTEN_DB_11,
    .default_width = ADC_BITWIDTH_DEFAULT
};

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
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

void ADC_init() {
    // Initialize ADC configuration here
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &ctx.adc1_handle));

    //-------------ADC1 Config---------------//
    const adc_oneshot_chan_cfg_t config = {
        .bitwidth = ctx.default_width,
        .atten = ctx.default_atten,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(ctx.adc1_handle, ctx.default_channel, &config));

    //-------------ADC1 Calibration Init---------------//
    ctx.calibrated = adc_calibration_init(ADC_UNIT_1, ctx.default_channel, ctx.default_atten, &ctx.adc1_cali_handle);
}

Millivolt ADC_read() {
    // Initialize variables
    Millivolt meas;
    unsigned sample = 0;
    unsigned samples = 100;
    unsigned step = 10;

    int voltage = 0;
    unsigned sum = 0;

    while (sample < samples) {
        // Read ADC value
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(ctx.adc1_handle, ctx.default_channel, &adc_value));

        if (ctx.calibrated) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(ctx.adc1_cali_handle, adc_value, &voltage));
        }

        sum += voltage;
        sample += 1;
        vTaskDelay(pdMS_TO_TICKS(step));
    }

    // Calculate average
    meas = sum / samples;
    ESP_LOGD(__func__, "ADC%d Channel[%d] Cali Voltage Avg: %d mV", ADC_UNIT_1 + 1, ctx.default_channel, meas);

    return meas;
}

AdcMeas ADC_read_for(Seconds duration) {
    AdcMeas meas;
    // Initialize variables
    unsigned time = 0;
    unsigned endTime = duration * 1000;
    unsigned step = 1000;

    int voltage = 0;
    unsigned sum = 0;
    meas.min = ADC_MAX_VALUE;
    meas.max = 0;

    // Perform measurements for the specified duration
    while (time < endTime) {
        // Read ADC value
        voltage = ADC_read();
        // Update sum, min, and max
        sum += voltage;
        if (voltage < meas.min)
            meas.min = voltage;
        
        if (voltage > meas.max)
            meas.max = voltage;

        time += step;
        vTaskDelay(pdMS_TO_TICKS(step));
    }

    // Calculate average
    meas.avg = sum / (endTime / step);  // Calculate average over the measurement period
    return meas;
}


static void example_adc_calibration_deinit(adc_cali_handle_t handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(__func__, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(__func__, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

void ADC_deinit() {
    ESP_ERROR_CHECK(adc_oneshot_del_unit(ctx.adc1_handle));
    if (ctx.calibrated) {
        example_adc_calibration_deinit(ctx.adc1_cali_handle);
    }
}