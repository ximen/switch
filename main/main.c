#include "esp_err.h"
#include "esp_log.h"
#include "app_config.h"

#define TAG "MAIN"

void app_main(void){
    ESP_ERROR_CHECK(app_config_init());		    // Initializing and loading configuration
}