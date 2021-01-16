#include "esp_log.h"
#include "app_common.h"
#include "app_config.h"
#include "app_board.h"
#include "app_mqtt.h"
#include "freertos/FreeRTOS.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_networking_api.h"

#define TAG "APP_COMMON"

#define TASK_STACK_SIZE 4096
#define QUEUE_LENGTH    5

const portTickType xTicksToWait = 1000 / portTICK_RATE_MS;
static xQueueHandle state_queue;

void queue_value(uint8_t channel, uint8_t value){
    queue_value_t item;
    item.channel = channel;
    item.state = value;
    portBASE_TYPE status = xQueueSend(state_queue, &item, xTicksToWait);
    if (status != pdPASS){
        ESP_LOGE(TAG, "Error queuing state");
    }
}

void notify(queue_value_t state){
    bool config_mesh_enable;
    bool config_mqtt_enable;
    app_config_getBool("ble_mesh_enable", &config_mesh_enable);
    app_config_getBool("mqtt_enable", &config_mqtt_enable);
    if (config_mesh_enable) {
        ESP_LOGI(TAG, "BLE Mesh enabled, notifying");
        esp_ble_mesh_elem_t *element = esp_ble_mesh_find_element(esp_ble_mesh_get_primary_element_address() + state.channel + 1);
        esp_ble_mesh_model_t *model = esp_ble_mesh_find_sig_model(element, ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV);
        esp_ble_mesh_server_state_value_t value = {.gen_onoff.onoff = state.state};
        ESP_LOGI(TAG, "Updating server value. Element: %d, Model: %d", element->element_addr, model->model_idx);
        esp_ble_mesh_server_model_update_state(model, ESP_BLE_MESH_GENERIC_ONOFF_STATE, &value);
    }
    if (config_mqtt_enable){
        app_mqtt_notify_status(state);
    }
}

static void worker_task( void *pvParameters ){
    for (;;){
        queue_value_t item;
        portBASE_TYPE xStatus = xQueueReceive(state_queue, &item, portMAX_DELAY );
        if( xStatus == pdPASS ){
            ESP_LOGI(TAG, "Received from queue: channel=%d, value=%d", item.channel, item.state);
            if (item.state == 0) app_board_off_channel(item.channel); else app_board_on_channel(item.channel);
            notify(item);
        }
        else {
            ESP_LOGI(TAG, "Queue receive timed out");
        }        
    }
}

void app_init_queue_worker(){
    state_queue = xQueueCreate(QUEUE_LENGTH, sizeof(queue_value_t));
    if(!state_queue){
        ESP_LOGE(TAG, "Error creating queue");
        return;
    }
    if (xTaskCreate(worker_task, "Worker1", TASK_STACK_SIZE, NULL, 1, NULL ) != pdPASS){
        ESP_LOGE(TAG, "Error creating worker task");
        return;
    }
}