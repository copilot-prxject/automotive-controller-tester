#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stdbool.h>

#include "modules/base/types.h"

bool PWM_init(void);

bool PWM_trigger_for(Seconds duration, Herz freq, Percent duty);
bool PWM_set_duty(Percent duty);
bool PWM_set_freq(Herz freq);

bool PWM_stop(void);

#endif // PWM_H