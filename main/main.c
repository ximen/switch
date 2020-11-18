#include "esp_err.h"
#include "esp_log.h"
#include "app_config.h"
#include "app_config_ble_mesh.h"
#include "esp_ble_mesh_generic_model_api.h"
#define TAG "MAIN"

static void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param){
    ESP_LOGI(TAG, "Callback stub");
}

void app_main(void){
    ESP_ERROR_CHECK(app_config_init());		    // Initializing and loading configuration
    esp_err_t err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "bluetooth_init failed (err %d)", err);
        return;
    }
    esp_ble_mesh_register_generic_server_callback(example_ble_mesh_generic_server_cb);
    app_config_ble_mesh_init();
}