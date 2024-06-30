#ifndef CT_H
#define CT_H

#include <stdint.h>

#include "modules/base/types.h"


void CT_init(void);

Amper CT_read(void);
void CT_read_for(Seconds duration);

void CT_deinit(void);

#endif // CT_H