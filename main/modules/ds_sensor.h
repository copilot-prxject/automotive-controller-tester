#ifndef DS_SENSOR_H
#define DS_SENSOR_H

#include <stdint.h>
#include "modules/base/types.h"


typedef struct {
    float first_sensor;
    float second_sensor;
} Temperatures;

void DS_SENSOR_init(void);

Temperatures DS_SENSOR_read(void);
void DS_SENSOR_read_for(Seconds duration);

void DS_SENSOR_deinit(void);

#endif  // DS_SENSOR_H