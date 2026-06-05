#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include <acv.h>

#define TWAI_TX_PIN GPIO_NUM_4
#define TWAI_RX_PIN GPIO_NUM_5
#define TWAI_BITRATE 500000

#define TWAI_OBD2_PID_MONITOR_STATUS 1 << 31


typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
} can_msg_t;

static QueueHandle_t msg_queue;

static twai_node_handle_t node;

static bool twai_receive_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx) {
    can_msg_t out;
    twai_frame_t msg = {
        .buffer = out.data,
        .buffer_len = sizeof(out.data),
    };

    if (ESP_OK == twai_node_receive_from_isr(handle, &msg)) {
        out.id = msg.header.id;
        out.len = msg.buffer_len;

        // TODO: filter here?

        BaseType_t hp_task_woken = pdFALSE;
        xQueueSendFromISR(msg_queue, &out, &hp_task_woken);

        return hp_task_woken == pdTRUE;
    }

    return false;
}

void twai_rx_task(void *arg) {
    can_msg_t msg;

    // bool x18 = false;
    // bool x60 = false;
    // bool x100 = false;
    // bool x110 = false;
    // bool x120 = false;
    // bool x121 = false;
    // bool x222 = false;

    while (1) {
        if (xQueueReceive(msg_queue, &msg, portMAX_DELAY)) {
            // if (msg.id == 0x18 && !x18) {
            //     x18 = true;
            //     ESP_LOGI("acv", "seen 0x18: brake and unknown");
            //     continue;
            // }
            // if (msg.id == 0x60 && !x60) {
            //     x60 = true;
            //     ESP_LOGI("acv", "seen 0x60: mode button");
            //     continue;
            // }
            // if (msg.id == 0x100 && !x100) {
            //     x100 = true;
            //     ESP_LOGI("acv", "seen 0x100: rpm");
            //     continue;
            // }
            // if (msg.id == 0x110 && !x110) {
            //     x110 = true;
            //     ESP_LOGI("acv", "seen 0x100: speed");
            //     continue;
            // }
            // if (msg.id == 0x120 && !x120) {
            //     x120 = true;
            //     ESP_LOGI("acv", "seen 0x120: coolant temp");
            //     continue;
            // }
            // if (msg.id == 0x121 && !x121) {
            //     x121 = true;
            //     ESP_LOGI("acv", "seen 0x121: gear");
            //     continue;>
            // }
            // if (msg.id == 0x222 && !x222) {
            //     x222 = true;
            //     ESP_LOGI("acv", "seen 0x222: unknown");
            //     continue;
            // }








            // if (msg.id == 0x100) {
            //     int16_t test_rpm = ((uint16_t)msg.data[0] << 8) | msg.data[1];
            //     ESP_LOGI("acv", "test_rpm: %d", test_rpm);
            //     continue;
            // }

            // if (msg.id == 0x121) {
            //     uint8_t gear = msg.data[0];
            //     uint8_t neutral = msg.data[1];
            //     ESP_LOGI("acv", "gear: %d %d", gear, neutral);
            //     continue;
            // }
            
            // if (msg.id == 0x100 || msg.id == 0x120 || msg.id == 0x121 || msg.id == 0x222) {
            //     // NOTE: 0x120 had some data lol
            //     // 120: 1 C 0 B8 0 0 0 0 0
            //     // 120: 1 C 0 B8 8 0 0 0 0
            //     // 120: 1 A 0 B8 8 0 0 0 0
            //     // 120: 1 A 0 B8 0 0 0 0 0
            //     // 120: 1 A 0 B8 1 0 0 0 0
            //     continue;
            // }

            // ESP_LOGI(
            //     "acv",
            //     "RX: %02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            //     msg.id,
            //     msg.data[0],
            //     msg.data[1],
            //     msg.data[2],
            //     msg.data[3],
            //     msg.data[4],
            //     msg.data[5],
            //     msg.data[6],
            //     msg.data[7]
            // );
            
            if (msg.id == 0x7E8) {
                if (msg.data[1] == 0x41 && msg.data[2] == 0x05) {
                    int8_t coolant_temp = msg.data[3] - 40;
                    acv_msg_coolant_temp(coolant_temp);
                    continue;
                }
                
                if (msg.data[1] == 0x41 && msg.data[2] == 0x0C) {
                    float rpm = ((msg.data[3] << 8) | msg.data[4]) / 4.0f;
                    acv_msg_rpm(rpm);
                    continue;
                }
                
                if (msg.data[1] == 0x41 && msg.data[2] == 0x0D) {
                    int8_t speed = msg.data[3];
                    acv_msg_speed(speed);
                    continue;
                }
                
                if (msg.data[1] == 0x41 && msg.data[2] == 0x0F) {
                    int8_t intake_air_temp = msg.data[3] - 40;
                    acv_msg_intake_air_temp(intake_air_temp);
                    continue;
                }
                
                if (msg.data[1] == 0x41 && msg.data[2] == 0x11) {
                    float throttle_position = 0.392156863 * msg.data[3];
                    acv_msg_throttle_position(throttle_position);
                    continue;
                }
            }
        }
    }
}

void twai_tx_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t buf_rpm[8] = {0x02, 0x01, 0x0C, 0x55, 0x55, 0x55, 0x55, 0x55};
        twai_frame_t msg_rpm = {
            .header.id = 0x7DF,
            .header.ide = false,
            .buffer = buf_rpm,
            .buffer_len = sizeof(buf_rpm),
        };
        esp_err_t err = twai_node_transmit(node, &msg_rpm, 0);
        if (err != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err));
        }
        // ESP_ERROR_CHECK(twai_node_transmit_wait_all_done(node, -1));

        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t buf_coolant_temp[8] = {0x02, 0x01, 0x05, 0x55, 0x55, 0x55, 0x55, 0x55};
        twai_frame_t msg_coolant_temp = {
            .header.id = 0x7DF,
            .header.ide = false,
            .buffer = buf_coolant_temp,
            .buffer_len = sizeof(buf_coolant_temp),
        };
        esp_err_t err_coolant_temp = twai_node_transmit(node, &msg_coolant_temp, 0);
        if (err_coolant_temp != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err_coolant_temp));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t buf_speed[8] = {0x02, 0x01, 0x0D, 0x55, 0x55, 0x55, 0x55, 0x55};
        twai_frame_t msg_speed = {
            .header.id = 0x7DF,
            .header.ide = false,
            .buffer = buf_speed,
            .buffer_len = sizeof(buf_speed),
        };
        esp_err_t err_speed = twai_node_transmit(node, &msg_speed, 0);
        if (err_speed != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err_speed));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t buf_intake_air_temp[8] = {0x02, 0x01, 0x0F, 0x55, 0x55, 0x55, 0x55, 0x55};
        twai_frame_t msg_intake_air_temp = {
            .header.id = 0x7DF,
            .header.ide = false,
            .buffer = buf_intake_air_temp,
            .buffer_len = sizeof(buf_intake_air_temp),
        };
        esp_err_t err_intake_air_temp = twai_node_transmit(node, &msg_intake_air_temp, 0);
        if (err_intake_air_temp != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err_intake_air_temp));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        uint8_t buf_throttle_position[8] = {0x02, 0x01, 0x11, 0x55, 0x55, 0x55, 0x55, 0x55};
        twai_frame_t msg_throttle_position = {
            .header.id = 0x7DF,
            .header.ide = false,
            .buffer = buf_throttle_position,
            .buffer_len = sizeof(buf_throttle_position),
        };
        esp_err_t err_throttle_position = twai_node_transmit(node, &msg_throttle_position, 0);
        if (err_throttle_position != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err_throttle_position));
        }
    }
}

void initialize_twai() {
    msg_queue = xQueueCreate(32, sizeof(can_msg_t));

    node = NULL;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = TWAI_TX_PIN,
        .io_cfg.rx = TWAI_RX_PIN,
        .bit_timing.bitrate = TWAI_BITRATE,
        .tx_queue_depth = 5,
    };
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node));

    // TODO: filter here with twai filters?

    twai_event_callbacks_t callbacks = {
        .on_rx_done = twai_receive_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, NULL));

    ESP_ERROR_CHECK(twai_node_enable(node));

    xTaskCreate(twai_rx_task, "twai_rx_task", 4096, NULL, 10, NULL);
    xTaskCreate(twai_tx_task, "twai_tx_task", 4096, NULL, 10, NULL);
}
