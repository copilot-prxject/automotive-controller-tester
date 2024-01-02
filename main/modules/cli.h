#ifndef CLI_H
#define CLI_H

#include <stdint.h>

void CLI_init(void);

typedef int (*CliCallback)(int argc, char **argv);
void CLI_register_command(char *name, char *help, CliCallback callback);

#endif  // CLI_H