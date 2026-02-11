#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include "led.h"

#define RX_GPIO 5
#define BITRATE 1000000
#define RX_QUEUE_LENGTH 50

static QueueHandle_t rx_queue;

static bool IRAM_ATTR rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *data, void *ctx) {
    twai_frame_t frame;

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        xQueueSendFromISR(rx_queue, &frame, NULL);
    }

    led_flash();

    return false;
}

static bool IRAM_ATTR error_callback(twai_node_handle_t handle, const twai_error_event_data_t *data, void *ctx) {
    ESP_EARLY_LOGW("acv", "TWAI error: 0x%x", data->err_flags.val);
    led_flash();
    return false;
}

void twai_task(void *task) {
    ESP_LOGI("acv", "twai: starting");

    rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(twai_frame_t));

    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = -1,
            .rx = RX_GPIO,
            .quanta_clk_out = -1,
            .bus_off_indicator = -1,
        },
        .bit_timing.bitrate = BITRATE,
        .flags.enable_listen_only = true,
    };

    twai_node_handle_t node;
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node));

    twai_event_callbacks_t callbacks = {
        .on_rx_done = rx_callback,
        .on_error = error_callback,
    };

    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, NULL));
    ESP_ERROR_CHECK(twai_node_enable(node));
    ESP_LOGI("acv", "twai: listening");

    twai_frame_t frame;

    while (1) {
        if (xQueueReceive(rx_queue, &frame, portMAX_DELAY)) {
            ESP_LOGI(
                "acv",
                "twai: received: ID=0x%X DLC=0x%X DATA=%02X %02X %02X %02X %02X %02X %02X %02X",
                frame.header.id,
                frame.header.dlc,
                frame.buffer[0],
                frame.buffer[1],
                frame.buffer[2],
                frame.buffer[3],
                frame.buffer[4],
                frame.buffer[5],
                frame.buffer[6],
                frame.buffer[7]
            );
        }
    }

    // while (1) {
    //     ESP_LOGI("acv", "twai tick");
    //     vTaskDelay(pdMS_TO_TICKS(500));
    // }
}
