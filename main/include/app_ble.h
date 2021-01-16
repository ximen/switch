#ifndef _APP_BLE_H_
#define _APP_BLE_H_

#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_config_model_api.h"

void app_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param);
void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param);

#endif /* _APP_BLE_H_ */