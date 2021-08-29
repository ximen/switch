#ifndef _APP_COMMON_H_
#define _APP_COMMON_H_

#include <stdint.h>
#include "app_config_mqtt_switch.h"
#include "app_board.h"

app_config_mqtt_switch_t *switches[CHANNEL_NUMBER];

typedef struct {
    uint8_t channel;
    uint8_t state;
} queue_value_t;

void queue_value(uint8_t channel, uint8_t value);
void notify(queue_value_t state);
void app_init_queue_worker();

#endif /* _APP_COMMON_H_ */