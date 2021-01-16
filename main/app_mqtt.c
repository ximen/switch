#include "esp_err.h"
#include "esp_log.h"
#include "app_mqtt.h"
#include "app_config.h"
#include "app_common.h"
#include "app_board.h"
#include "sdkconfig.h"

#define TAG "APP_MQTT"

#define ON_MSG          "ON"
#define OFF_MSG         "OFF"
#define ALLOC_ERR_STR   "Error allocating buffer!"
#define ONLINE_MSG      "online"
#define OFFLINE_MSG     "offline"
#define AVAIL_TOPIC     "/available"

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
    // your_context_t *context = event->context;
    switch (event->event_id) {
            case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            for(uint8_t i=0; i<CHANNEL_NUMBER; i++){
                char *topic = get_mqtt_topic(i);
                if(strlen(topic) > 0){
                    ESP_LOGI(TAG, "Subscribing %s", topic);
                    esp_mqtt_client_subscribe(client, topic, 1);
                }
                app_mqtt_notify_avail(i, ONLINE_MSG);
                queue_value_t state;
                state.channel = i;
                state.state = app_board_get_channel(i);
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
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
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

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void app_mqtt_get_lwt(char **topic, char **msg){
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
                *topic = avail_topic;
                *msg = OFFLINE_MSG;
            } else {
                ESP_LOGE(TAG, ALLOC_ERR_STR);
                free(base_path);
            }
        }
    }
}