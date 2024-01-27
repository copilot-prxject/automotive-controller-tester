#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h>

bool RELAY_init(void);

bool RELAY_set_state(bool state);

#endif // RELAY_H