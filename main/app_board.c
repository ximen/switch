#include "app_board.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define RESET_TIME_MS   3000
#define LED_PIN 		GPIO_NUM_2
#define BUTTON_PIN 		GPIO_NUM_0
#define ACTIVE_LEVEL 	1
#define ON_LEVEL		ACTIVE_LEVEL
#define OFF_LEVEL		!ACTIVE_LEVEL

static TimerHandle_t  reset_timer;

static gpio_num_t outputs[CHANNEL_NUMBER] = {
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

static void reset_timer_cb(TimerHandle_t xTimer){
    app_config_erase();
}

static void IRAM_ATTR gpio_isr_handler(void* arg){
    if (gpio_get_level(BUTTON_PIN) == 0){
        xTimerStartFromISR(reset_timer, 0);
    } else {
        xTimerStopFromISR(reset_timer, 0);
    }
}

void app_board_init(){
    reset_timer = xTimerCreate("reset_timer", RESET_TIME_MS / portTICK_PERIOD_MS, pdFALSE, NULL, reset_timer_cb);    

    gpio_reset_pin(BUTTON_PIN);
    gpio_pullup_dis(BUTTON_PIN);
    gpio_pulldown_dis(BUTTON_PIN);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void*) NULL);
}

void app_board_on_channel(uint8_t channel){
    gpio_set_level(channel, ON_LEVEL);
}

void app_board_off_channel(uint8_t channel){
    gpio_set_level(channel, OFF_LEVEL);
}

uint8_t app_board_get_channel(uint8_t channel){
    return gpio_get_level(outputs[channel]);
}