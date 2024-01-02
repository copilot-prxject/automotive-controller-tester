#include "cli.h"

#include "esp_console.h"
#include "esp_log.h"

#include "modules/base/types.h"

static struct {
    esp_console_repl_t *repl;
    esp_console_repl_config_t replConfig;
    esp_console_dev_uart_config_t uartConfig;
} ctx = {
    .repl = NULL,
    .replConfig = ESP_CONSOLE_REPL_CONFIG_DEFAULT(),
    .uartConfig = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT()
};

void CLI_register_command(char *name, char *help, CliCallback callback) {
    esp_console_cmd_t command_struct = {
        .command = name,
        .help = help,
        .func = callback,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&command_struct));
}

void CLI_init(void) {
    // esp_console_config_t console_config = {
    //     .max_cmdline_args = 8,
    //     .max_cmdline_length = 256,
    // };
    // ctx.replConfig = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    // ctx.uartConfig = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&ctx.uartConfig, &ctx.replConfig, &ctx.repl));
    // ESP_ERROR_CHECK(esp_console_init(&console_config));
    ESP_ERROR_CHECK(esp_console_start_repl(ctx.repl));
}
