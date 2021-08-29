#include "esp_err.h"
#include "esp_log.h"
#include "app_config.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
//#include "app_mqtt.h"
#include "app_ble.h"
#include "app_common.h"
#include "app_board.h"
#include "app_config_wifi.h"

#define TAG "MAIN"

bool mqtt_enable;
char *avail_topic;
app_config_cbs_t app_cbs;

void ip_cb(ip_event_t event, void *event_data){
    char *std_mqtt_prefix;
    app_config_getString("std_mqtt_prefix", &std_mqtt_prefix);
    char *std_mqtt_objid;
    app_config_getString("std_mqtt_objid", &std_mqtt_objid);
    int avail_topic_len = strlen(std_mqtt_prefix) + strlen(std_mqtt_objid) + strlen(CONFIG_APP_CONFIG_MQTT_SWITCH_AVAIL_STR) + 3;
    avail_topic = (char *)malloc(avail_topic_len);
    snprintf(avail_topic, avail_topic_len, "%s/%s%s", std_mqtt_prefix, std_mqtt_objid, CONFIG_APP_CONFIG_MQTT_SWITCH_AVAIL_STR);
    app_config_mqtt_lwt_t lwt = {.topic = avail_topic, .msg = "offline"};
    ESP_LOGI(TAG, "LWT topic: %s", lwt.topic);
    app_config_mqtt_init(&lwt);
}

void mqtt_switch_cmd_handler(uint8_t state, app_config_mqtt_switch_t *sw){
    ESP_LOGI(TAG, "Setting channel %d to %d", (int)sw->user_data, state);
    queue_value((int)sw->user_data, state);
}

void mqtt_cb(esp_mqtt_event_handle_t event){
    if(event->event_id == MQTT_EVENT_CONNECTED){
        app_config_mqtt_publish(avail_topic, "online", true);
        char *switch_prefix;
        app_config_getString("switch_prefix", &switch_prefix);
        for(int i=0; i<CHANNEL_NUMBER; i++){
            char *sw_name;
            char sw_obj[10] = {0};
            char sw_iter[15] = {0};
            snprintf(sw_iter, 15, "switch%d_name", (uint8_t)i+1);
            snprintf(sw_obj, 10, "switch%d", (uint8_t)i+1);
            //app_config_getString("light_prefix", &light_prefix);
            app_config_getString(sw_iter, &sw_name);
            if(strlen(sw_name))
                switches[i] = app_config_mqtt_switch_create(switch_prefix,
                                                    sw_obj,
                                                    sw_name,
                                                    mqtt_switch_cmd_handler,
                                                    true, true, (void *)i);
        }
    } else if(event->event_id == MQTT_EVENT_DISCONNECTED){
        free(avail_topic);
    }
}

void app_main(void){
    app_board_init();
    app_cbs.config_srv = app_ble_mesh_config_server_cb;
    app_cbs.generic_srv = example_ble_mesh_generic_server_cb;
    //app_cbs.mqtt = mqtt_event_handler;

    //app_mqtt_get_lwt(&app_cbs.lwt.topic, &app_cbs.lwt.msg);
    ESP_ERROR_CHECK(app_config_init(&app_cbs));		    // Initializing and loading configuration
    app_init_queue_worker();

    app_config_getBool("mqtt_enable", &mqtt_enable);    
    if(mqtt_enable){
        app_config_ip_register_cb(IP_EVENT_STA_GOT_IP, ip_cb);
        app_config_mqtt_register_cb(MQTT_EVENT_CONNECTED, mqtt_cb);
        app_config_mqtt_register_cb(MQTT_EVENT_DISCONNECTED, mqtt_cb);
    }
}