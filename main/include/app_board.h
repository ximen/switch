#ifndef _APP_BOARD_H_
#define _APP_BOARD_H_

#include <stdint.h>

#define CHANNEL_NUMBER  10

void app_board_init();
void app_board_on_channel(uint8_t channel);
void app_board_off_channel(uint8_t channel);
uint8_t app_board_get_channel(uint8_t channel);

#endif /* _APP_BOARD_H_ */