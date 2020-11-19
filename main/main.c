#include "esp_err.h"
#include "esp_log.h"
#include "app_config.h"
#include "app_config_ble_mesh.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define TASK_STACK_SIZE 4096
#define QUEUE_LENGTH    5
#define CHANNEL_NUMBER  10
#define LED_PIN 		GPIO_NUM_2
#define BUTTON_PIN 		GPIO_NUM_0
#define ACTIVE_LEVEL 	1
#define ON_LEVEL		ACTIVE_LEVEL
#define OFF_LEVEL		!ACTIVE_LEVEL
#define TAG "MAIN"

gpio_num_t outputs[CHANNEL_NUMBER] = {
		GPIO_NUM_12,
		GPIO_NUM_13,
		GPIO_NUM_14,
		GPIO_NUM_15,
		GPIO_NUM_16,
		GPIO_NUM_17,
		GPIO_NUM_18,
		GPIO_NUM_19,
		GPIO_NUM_21,
		GPIO_NUM_22
};

const portTickType xTicksToWait = 1000 / portTICK_RATE_MS;

xQueueHandle state_queue;

typedef struct {
    uint8_t channel;
    uint8_t state;
} queue_value_t;

void queue_value(uint8_t channel, uint8_t value){
    queue_value_t item;
    item.channel = channel;
    item.state = value;
    portBASE_TYPE status = xQueueSend(state_queue, &item, xTicksToWait);
    if (status != pdPASS){
        ESP_LOGE(TAG, "Error queuing state");
    }
}

uint8_t get_channel_number(esp_ble_mesh_model_t *model, esp_ble_mesh_msg_ctx_t *ctx){
    uint16_t primary_addr = esp_ble_mesh_get_primary_element_address();
    uint8_t elem_count = esp_ble_mesh_get_element_count();

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(ctx->recv_dst)) {
        for (uint8_t i = 0; i < elem_count; i++) {
            if (ctx->recv_dst == (primary_addr + i)) {
                return i - 1;
            }
        }
    } else if (ESP_BLE_MESH_ADDR_IS_GROUP(ctx->recv_dst)) {
        if (esp_ble_mesh_is_model_subscribed_to_group(model, ctx->recv_dst)) {
            return model->element->element_addr - primary_addr - 1;
        }
    } else if (ctx->recv_dst == 0xFFFF) {
        return model->element->element_addr - primary_addr;
    } else {
        ESP_LOGE(TAG, "Error looking for channel number");
    }
    return 0xFF;
} 

static void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param){
    esp_ble_mesh_gen_onoff_srv_t *srv;
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04x, src 0x%04x, dst 0x%04x",
        event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            ESP_LOGI(TAG, "onoff 0x%02x", param->value.state_change.onoff_set.onoff);
            uint8_t channel = get_channel_number(param->model, &param->ctx);
            if (channel < CHANNEL_NUMBER) queue_value(channel, param->value.state_change.onoff_set.onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            srv = param->model->user_data;
            ESP_LOGI(TAG, "onoff 0x%02x", srv->state.onoff);
            uint8_t channel = get_channel_number(param->model, &param->ctx);
            ESP_LOGI(TAG, "Received get, channel %d", channel);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            ESP_LOGI(TAG, "onoff 0x%02x, tid 0x%02x", param->value.set.onoff.onoff, param->value.set.onoff.tid);
            if (param->value.set.onoff.op_en) {
                ESP_LOGI(TAG, "trans_time 0x%02x, delay 0x%02x",
                    param->value.set.onoff.trans_time, param->value.set.onoff.delay);
                ESP_LOGI(TAG, "Received set");
            }
        }
        break;
    default:
        ESP_LOGE(TAG, "Unknown Generic Server event 0x%02x", event);
        break;
    }
}

static void worker_task( void *pvParameters ){
    for (;;){
        queue_value_t item;
        portBASE_TYPE xStatus = xQueueReceive(state_queue, &item, portMAX_DELAY );
        if( xStatus == pdPASS ){
            ESP_LOGI(TAG, "Received from queue: channel=%d, value=%d", item.channel, item.state);
            if (item.state == 0) gpio_set_level(item.channel, OFF_LEVEL); else gpio_set_level(item.channel, ON_LEVEL);
        }
        else {
            ESP_LOGI(TAG, "Queue receive timed out");
        }        
    }
}

void app_main(void){
    ESP_ERROR_CHECK(app_config_init());		    // Initializing and loading configuration
    esp_err_t err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "bluetooth_init failed (err %d)", err);
        return;
    }
    esp_ble_mesh_register_generic_server_callback(example_ble_mesh_generic_server_cb);
    state_queue = xQueueCreate(QUEUE_LENGTH, sizeof(queue_value_t));
    if(!state_queue){
        ESP_LOGE(TAG, "Error creating queue");
        return;
    }
    xTaskCreate(worker_task, "Worker1", TASK_STACK_SIZE, NULL, 1, NULL );
    app_config_ble_mesh_init();
}