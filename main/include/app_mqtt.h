#ifndef _APP_MQTT_H_
#define _APP_MQTT_H_

#include "app_common.h"
#include "mqtt_client.h"

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void app_mqtt_notify_status(queue_value_t state);
void app_mqtt_get_lwt(char **topic, char **msg);

#endif /* _APP_MQTT_H_ */