#include "modules/pwm.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"

#include "driver/ledc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/timers.h>

#include "modules/base/generic_fun.h"
#include "modules/cli.h"
#include "modules/ble.h"


static struct {
    Herz freq;

    bool ongoing;
    TimerHandle_t timer_duration;

    unsigned speed_mode;
    unsigned timer_num;
    unsigned timer_resolution;
    unsigned channel;
    unsigned pin;
} ctx = {
    .freq               = 1000,
    .speed_mode         = LEDC_HIGH_SPEED_MODE,
    .timer_num          = LEDC_TIMER_0,
    .timer_resolution   = LEDC_TIMER_13_BIT,
    .channel            = LEDC_CHANNEL_0,
    // .pin                = GPIO_NUM_27,
    .pin                = GPIO_NUM_33,
};

// pwm duration 1000 duty 90 freq 10

static unsigned GetDutyResolutionFromPercent(Percent duty) {
    if (duty > 100) {
        ESP_LOGW(__func__, "Duty out of range. Readjusted.");
        duty = 100;
    }

    return ((1 << ctx.timer_resolution) - 1) * duty / 100;
}

static void StopTimer(void) {
    xTimerDelete(ctx.timer_duration, 0);
    ctx.ongoing = false;
}

void timer_callback(TimerHandle_t xTimer) {
    PWM_stop();
    StopTimer();
}

static bool FoundUnsignedArgument(int argc, char **argv, const char *arg, unsigned *val) {
    for (int i = 1; i < argc; i++) {
        if (AreStringsTheSame(arg, argv[i], strlen(arg)) == false)
            continue;

        if (val != NULL) {
            char *ptr;
            *val = strtol(argv[i + 1], &ptr, 10);
            if (strlen(ptr) != 0 ) {
                ESP_LOGW(__func__, "Invalid value for argument: %s", argv[i]);
                return false;
            }
        }
        return true;
    }
    return false;
}

static int pwm_command_execution(int argc, char **argv) {
    if (argc == 1){
        ESP_LOGI(__func__, "No arguments");
        return 0;
    }

    static const char force[] = "force";
    if (FoundUnsignedArgument(argc, argv, force, NULL) && ctx.ongoing) {
        PWM_stop();
        StopTimer();
    }

    unsigned durat = 1;
    static const char duration[] = "duration";
    if (FoundUnsignedArgument(argc, argv, duration, &durat) == false)
        return 0;

    unsigned frequ = 0;
    static const char freq[] = "freq";
    if (FoundUnsignedArgument(argc, argv, freq, &frequ) == false)
        return 0;

    unsigned duty_ = 101;
    static const char duty[] = "duty";
    if (FoundUnsignedArgument(argc, argv, duty, &duty_) == false)
        return 0;

    ctx.ongoing = PWM_trigger_for(durat, frequ, duty_);
    ESP_LOGI(__func__, "PWM: %s", ctx.ongoing ? "ongoing" : "errors occurs");
    return 0;
}

static int update_command_execution(int argc, char **argv) {
    if (argc == 1){
        ESP_LOGI(__func__, "No arguments");
        return 0;
    }

    unsigned frequ = 0;
    static const char freq[] = "freq";
    if (FoundUnsignedArgument(argc, argv, freq, &frequ)) {
        PWM_set_freq(frequ);
    }

    unsigned duty_ = 101;
    static const char duty[] = "duty";
    if (FoundUnsignedArgument(argc, argv, duty, &duty_)) {
        PWM_set_duty(duty_);
    }

    return 0;
}

// static void getValuesFromString(char *buffer, ) {

// }

static void parse_ble_command(char *buffer, unsigned length) {
    unsigned values[4] = { 0 };
    unsigned i = 0;
    char *end = buffer;
    while(*end) {
        values[i] = strtoul(buffer, &end, 10);
        while (*end == ',')
            end++;
        
        buffer = end;
        ++i;
    }

    if (values[0] != 0) {    // force
        PWM_stop();
        StopTimer();
    }

    if (values[1] != 0) {
        ctx.freq = values[3]; 
        PWM_trigger_for(values[1], values[2], ctx.freq);
    }
}

bool PWM_init(void) {
    ledc_timer_config_t pwm_timer = {
        .speed_mode = ctx.speed_mode,
        .duty_resolution = ctx.timer_resolution,
        .timer_num = ctx.timer_num,
        .freq_hz = ctx.freq,
    };
    bool ret = (ESP_OK == ledc_timer_config(&pwm_timer));

    ledc_channel_config_t pwm_channel = {
        .gpio_num = ctx.pin,
        .speed_mode = ctx.speed_mode,
        .channel = ctx.channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = ctx.timer_num,
        .duty = 0,   // Initial duty cycle (0 to 2^PWM_LEDC_TIMER_BIT - 1)
        .hpoint = 0,
    };

    ret = ret && (ESP_OK == ledc_channel_config(&pwm_channel));
    if (ret) {
        CLI_register_command("pwm", "[force] [duration <time>] [duty <duty>] [freq <frequency>]", pwm_command_execution);
        CLI_register_command("pwm-update", "[duty <duty>] [freq <frequency>]", update_command_execution);
        BLE_setup_characteristic_callback(kPWM, parse_ble_command);
    }
    return ret;
}

bool PWM_trigger_for(Seconds duration, Percent duty, Herz freq) {
    ctx.ongoing = true;
    ctx.timer_duration = xTimerCreate("PWMTimer",
                            pdMS_TO_TICKS(1000 * duration),  // Timer period in milliseconds
                            pdFALSE,                // Auto-reload
                            0,                      // Timer ID (not used here)
                            timer_callback);        // Callback function

    if (ctx.timer_duration == NULL)
        ESP_LOGW(__func__, "Creating timer for PWM failed. Stop PWM manually!");

    if (xTimerStart(ctx.timer_duration, 0) != pdPASS)
        ESP_LOGW(__func__, "Starting timer for PWM failed. Stop PWM manually!");

    return ESP_OK == ledc_set_freq(ctx.speed_mode, ctx.timer_num, freq)
        && ESP_OK == ledc_set_duty(ctx.speed_mode, ctx.channel, GetDutyResolutionFromPercent(duty))
        && ESP_OK == ledc_update_duty(ctx.speed_mode, ctx.channel);
}

bool PWM_set_duty(Percent duty) {
    return ESP_OK == ledc_set_duty(ctx.speed_mode, ctx.channel, GetDutyResolutionFromPercent(duty))
        && ESP_OK == ledc_update_duty(ctx.speed_mode, ctx.channel);
}

bool PWM_set_freq(Herz freq) {
    return ESP_OK == ledc_set_freq(ctx.speed_mode, ctx.timer_num, freq);
}

bool PWM_stop(void) {
    return ESP_OK == ledc_stop(ctx.speed_mode, ctx.channel, 0);
}
