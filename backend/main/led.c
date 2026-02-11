#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define LED_GPIO GPIO_NUM_23
#define LED_GPIO_DELAY 500
#define LED_GPIO_ON 0
#define LED_GPIO_OFF 1

#define LED_QUEUE_LENGTH 10

static QueueHandle_t led_queue;

void led_flash() {
    uint8_t sig = 1;
    xQueueSend(led_queue, &sig, 0);
}

void led_task(void *task) {
    ESP_LOGI("acv", "starting led task");

    led_queue = xQueueCreate(LED_QUEUE_LENGTH, sizeof(uint8_t));

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, LED_GPIO_OFF);

    uint8_t sig;
    const TickType_t HEARTBEAT_TICKS = pdMS_TO_TICKS(500);

    while (1) {
        if (xQueueReceive(led_queue, &sig, HEARTBEAT_TICKS) == pdTRUE) {
            gpio_set_level(LED_GPIO, LED_GPIO_ON);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_GPIO, LED_GPIO_OFF);
        } else {
            gpio_set_level(LED_GPIO, LED_GPIO_ON);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(LED_GPIO, LED_GPIO_OFF);
        }
    }
}
