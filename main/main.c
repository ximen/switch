#include "esp_err.h"
#include "esp_log.h"
#include "app_config.h"
#include "app_config_ble_mesh.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "mqtt_client.h"

#define TASK_STACK_SIZE 4096
#define QUEUE_LENGTH    5
#define CHANNEL_NUMBER  10
#define LED_PIN 		GPIO_NUM_2
#define BUTTON_PIN 		GPIO_NUM_0
#define ACTIVE_LEVEL 	1
#define ON_LEVEL		ACTIVE_LEVEL
#define OFF_LEVEL		!ACTIVE_LEVEL
#define TAG "MAIN"
#define RESET_TIME_MS   3000
#define ALLOC_ERR_STR   "Error allocating buffer!"
#define ONLINE_MSG      "online"
#define OFFLINE_MSG     "offline"
#define AVAIL_TOPIC     "/available"
#define ON_MSG          "ON"
#define OFF_MSG         "OFF"

//static esp_mqtt_client_handle_t client;
app_config_cbs_t app_cbs;
TimerHandle_t   reset_timer;

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

void reset_timer_cb(TimerHandle_t xTimer){
    app_config_erase();
}

static void IRAM_ATTR gpio_isr_handler(void* arg){
    if (gpio_get_level(BUTTON_PIN) == 0){
        xTimerStartFromISR(reset_timer, 0);
    } else {
        xTimerStopFromISR(reset_timer, 0);
    }
}

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

static void app_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param){
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT){
        switch (param->ctx.recv_op) {
            case ESP_BLE_MESH_MODEL_OP_NODE_RESET:
                ESP_LOGW(TAG, "Resetting Ble mesh node!");
                esp_ble_mesh_node_local_reset();
                break;
            default:
                break;
        }
        
    }
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

char *get_mqtt_topic(uint8_t channel){
    char *topic;
    char topic_name[CONFIG_APP_CONFIG_SHORT_NAME_LEN];
    sprintf(topic_name, "topic%d_element", channel + 1);
    esp_err_t err = app_config_getValue(topic_name, string, &topic);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Error retrieving topic %s", topic_name);
        return NULL;
    }
    return topic;
}

void app_mqtt_notify_status(queue_value_t state){
        char *topic = get_mqtt_topic(state.channel);
        char status_topic[58] = {0};
        strncat(status_topic, topic, 50);
        strcat(status_topic, "/status");
        if (strlen(topic) > 0){
            ESP_LOGI(TAG, "Publishing MQTT status. Topic %s, value %d", topic, state.state);
            if(state.state) app_config_mqtt_publish(status_topic, ON_MSG, 1);
            else app_config_mqtt_publish(status_topic, OFF_MSG, 1);
        }
        else{
            ESP_LOGI(TAG,"Topic not specified");
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
            if (item.state == 0) gpio_set_level(item.channel, OFF_LEVEL); else gpio_set_level(item.channel, ON_LEVEL);
            notify(item);
        }
        else {
            ESP_LOGI(TAG, "Queue receive timed out");
        }        
    }
}

void app_mqtt_notify_avail(uint8_t channel, char* msg){
    char element[17] = {0};
    sprintf(element, "topic%d_element", channel + 1);
    char *base_path;
    app_config_getValue(element, string, &base_path);
    if(strlen(base_path)){
        char *avail_topic = calloc(strlen(base_path) + sizeof(AVAIL_TOPIC) + 1, sizeof(char));
        if (avail_topic) {
            strncat(avail_topic, base_path, strlen(base_path) + 1);
            strncat(avail_topic, AVAIL_TOPIC, sizeof(AVAIL_TOPIC) + 1);
            app_config_mqtt_publish(avail_topic, msg, true);
        } else {
            ESP_LOGE(TAG, ALLOC_ERR_STR);
            free(base_path);
        }
        free(avail_topic);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event){
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
            case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            for(uint8_t i=0; i<CHANNEL_NUMBER; i++){
                char *topic = get_mqtt_topic(i);
                if(strlen(topic) > 0){
                    ESP_LOGI(TAG, "Subscribing %s", topic);
                    msg_id = esp_mqtt_client_subscribe(client, topic, 1);
                }
                app_mqtt_notify_avail(i, ONLINE_MSG);
                queue_value_t state;
                state.channel = i;
                state.state = gpio_get_level(outputs[i]);
                app_mqtt_notify_status(state);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            for(uint8_t i=0; i < CHANNEL_NUMBER; i++){
                char *topic = get_mqtt_topic(i);
                if(strlen(topic) == 0){
                    ESP_LOGI(TAG, "Empty topic %d", i);
                    continue;
                }
                if(strncmp(event->topic, topic, event->topic_len) == 0){
                    if(strncmp(event->data, ON_MSG, event->data_len) == 0){
                        ESP_LOGI(TAG, "Got ON");
                        queue_value(i, 1);
                    } else if (strncmp(event->data, OFF_MSG, event->data_len) == 0){
                        ESP_LOGI(TAG, "Got OFF");
                        queue_value(i, 0);
                    } else {
                        ESP_LOGW(TAG, "Error parsing payload");
                    }
                    break;
                }
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void app_main(void){
    reset_timer = xTimerCreate("reset_timer", RESET_TIME_MS / portTICK_PERIOD_MS, pdFALSE, NULL, reset_timer_cb);
    gpio_reset_pin(BUTTON_PIN);
    gpio_pullup_dis(BUTTON_PIN);
    gpio_pulldown_dis(BUTTON_PIN);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void*) NULL);

    for (uint8_t i = 0; i < CHANNEL_NUMBER; i++){
        char element[17] = {0};
        sprintf(element, "topic%d_element", i + 1);
        char *base_path;
        app_config_getValue(element, string, &base_path);
        if(strlen(base_path)){
            char *avail_topic = calloc(strlen(base_path) + sizeof(AVAIL_TOPIC) + 1, sizeof(char));
            if (avail_topic) {
                strncat(avail_topic, base_path, strlen(base_path) + 1);
                strncat(avail_topic, AVAIL_TOPIC, sizeof(AVAIL_TOPIC) + 1);
                app_cbs.lwt.topic = avail_topic;
                app_cbs.lwt.msg = OFFLINE_MSG;
            } else {
                ESP_LOGE(TAG, ALLOC_ERR_STR);
                free(base_path);
            }
        }
    }

    app_cbs.config_srv = app_ble_mesh_config_server_cb;
    app_cbs.generic_srv = example_ble_mesh_generic_server_cb;
    app_cbs.mqtt = mqtt_event_handler;

    ESP_ERROR_CHECK(app_config_init(&app_cbs));		    // Initializing and loading configuration
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