idf_component_register(SRCS "main.c"
                        "modules/cli.c"
                        "modules/adc.c"
                        "modules/pwm.c"

                    SRC_DIRS "./modules" "./modules/base"
                    INCLUDE_DIRS ".")