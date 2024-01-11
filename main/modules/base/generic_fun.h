#ifndef GENERIC_FUN_H
#define GENERIC_FUN_H

#include <stdint.h>
#include <stdbool.h>

#include "modules/base/types.h"

// bool AreStringsTheSame(const char *first, const char *second, unsigned length);
bool AreStringsTheSame(const char *first, const char *second, unsigned length) {
    return strncmp(first, second, length) == 0;
}

#endif // GENERIC_FUN_H
