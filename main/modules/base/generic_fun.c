#include "modules/base/generic_fun.h"

#include <string.h>

bool AreStringsTheSame(const char *first, const char *second, unsigned length) {
    return strncmp(first, second, length) == 0;
}
