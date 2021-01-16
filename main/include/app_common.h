#ifndef _APP_COMMON_H_
#define _APP_COMMON_H_

#include <stdint.h>

typedef struct {
    uint8_t channel;
    uint8_t state;
} queue_value_t;

void queue_value(uint8_t channel, uint8_t value);
void notify(queue_value_t state);
void app_init_queue_worker();

#endif /* _APP_COMMON_H_ */