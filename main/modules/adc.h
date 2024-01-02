#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#include "modules/base/types.h"

// Define constants
#define ADC_MAX_VALUE 4095
#define ADC_REF_VOLTAGE 3.3

typedef struct {
    Millivolt min;
    Millivolt max;
    Millivolt avg;
} AdcMeas;

void ADC_init(void);

Millivolt ADC_read(void);
void ADC_read_for(Seconds duration);

void ADC_deinit();

#endif // ADC_H