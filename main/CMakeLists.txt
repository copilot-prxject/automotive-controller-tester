file(GLOB_RECURSE SOURCES "*.c")

set(INCLUDES "." "./modules/base")
set(DEPENDENCIES "console" "bt" "driver" "esp_adc" "nvs_flash")

idf_component_register(SRCS ${SOURCES} INCLUDE_DIRS ${INCLUDES} REQUIRES ${DEPENDENCIES})