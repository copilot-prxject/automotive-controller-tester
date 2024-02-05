#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#include "modules/base/types.h"


void ADC_init(void);

Millivolt ADC_read(void);
void ADC_read_for(Seconds duration);

void ADC_deinit(void);

#endif // ADC_H