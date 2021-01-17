#include "esp_err.h"
#include "esp_log.h"
#include "app_config.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "app_mqtt.h"
#include "app_ble.h"
#include "app_common.h"
#include "app_board.h"

#define TAG "MAIN"

app_config_cbs_t app_cbs;

void app_main(void){
    app_board_init();
    app_cbs.config_srv = app_ble_mesh_config_server_cb;
    app_cbs.generic_srv = example_ble_mesh_generic_server_cb;
    app_cbs.mqtt = mqtt_event_handler;

    app_mqtt_get_lwt(&app_cbs.lwt.topic, &app_cbs.lwt.msg);
    ESP_ERROR_CHECK(app_config_init(&app_cbs));		    // Initializing and loading configuration
    app_init_queue_worker();
}