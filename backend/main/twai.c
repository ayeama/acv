#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include <acv.h>

#define TWAI_TX_PIN GPIO_NUM_4
#define TWAI_RX_PIN GPIO_NUM_5
#define TWAI_BITRATE 500000

#define FUNCTIONAL_ADDRESS_BASE 0x7DF
#define FUNCTIONAL_ADDRESS_EXTENDED 0x18DB33F1

static const uint8_t PID_SUPPORTED[] = {0x02, 0x01, 0x00};
static const uint8_t PID_COOLANT_TEMP[] = {0x02, 0x01, 0x05};
static const uint8_t PID_RPM[] = {0x02, 0x01, 0x0C};
static const uint8_t PID_SPEED[] = {0x02, 0x01, 0x0D};
static const uint8_t PID_INTAKE_AIR_TEMP[] = {0x02, 0x01, 0x0F};
static const uint8_t PID_THROTTLE_POSITION[] = {0x02, 0x01, 0x11};

static bool identifier_base_seen = false;
static bool identifier_extended_seen = false;

static uint32_t supported_pids = 0;

typedef struct {
    uint32_t id;
    uint32_t ide;
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
        out.ide = msg.header.ide;
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
            if (
                !msg.ide &&
                (msg.id >= 0x7E8 && msg.id <= 0x7EF) &&
                msg.data[1] == 0x41 &&
                msg.data[2] == 0x00
            ) {
                identifier_base_seen = true;
                supported_pids =
                    ((uint32_t)msg.data[3] << 24) |
                    ((uint32_t)msg.data[4] << 16) |
                    ((uint32_t)msg.data[5] << 8) |
                    ((uint32_t)msg.data[6]);
                continue;
            }
            if (
                msg.ide &&
                (msg.id >= 0x18DAF100 && msg.id <= 0x18DAF1FF) &&
                msg.data[1] == 0x41 &&
                msg.data[2] == 0x00
            ) {
                identifier_extended_seen = true;
                supported_pids =
                    ((uint32_t)msg.data[3] << 24) |
                    ((uint32_t)msg.data[4] << 16) |
                    ((uint32_t)msg.data[5] << 8) |
                    ((uint32_t)msg.data[6]);
                continue;
            }

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

            if (
                (!msg.ide && (msg.id >= 0x7E8 && msg.id <= 0x7EF)) ||
                (msg.ide && (msg.id >= 0x18DAF100 && msg.id <= 0x18DAF1FF))
            ) {
                if (msg.data[1] == 0x41) {
                    switch (msg.data[2]) {
                        case 0x05:
                            int8_t coolant_temp = msg.data[3] - 40;
                            acv_msg_coolant_temp(coolant_temp);
                            continue;
                        case 0x0C:
                            uint16_t rpm = (((uint16_t)msg.data[3] << 8) | msg.data[4]) / 4;
                            acv_msg_rpm(rpm);
                            continue;
                        case 0x0D:
                            int8_t speed = msg.data[3];
                            acv_msg_speed(speed);
                            continue;
                        case 0x0F:
                            int8_t intake_air_temp = msg.data[3] - 40;
                            acv_msg_intake_air_temp(intake_air_temp);
                            continue;
                        case 0x11:
                            float throttle_position = 0.392156863 * msg.data[3];
                            acv_msg_throttle_position(throttle_position);
                            continue;
                        default:
                            continue;
                    }
                }
            }
        }
    }
}

void discover_identifier() {
    while (1) {
        esp_err_t err;

        ESP_LOGI("ACV", "attempting to discover CAN bus identifier...");

        twai_frame_t msg_base = {
            .header.id = FUNCTIONAL_ADDRESS_BASE,
            .header.ide = false,
            .buffer = (uint8_t *)PID_SUPPORTED,
            .buffer_len = sizeof(PID_SUPPORTED),
        };
        err = twai_node_transmit(node, &msg_base, 0);
        if (err != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err));
        }
        err = twai_node_transmit_wait_all_done(node, 500);
        if (err == ESP_OK) {
            if (identifier_base_seen) {
                ESP_LOGI("ACV", "CAN bus discovered base identifier, supported pids: %#b", supported_pids);
                return;
            }
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGW("ACV", "CAN bus timed out attemping to discover base identifier: %s", esp_err_to_name(err));
        } else if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err));
        }

        twai_frame_t msg_extended = {
            .header.id = FUNCTIONAL_ADDRESS_EXTENDED,
            .header.ide = true,
            .buffer = (uint8_t *)PID_SUPPORTED,
            .buffer_len = sizeof(PID_SUPPORTED),
        };
        err = twai_node_transmit(node, &msg_extended, 0);
        if (err != ESP_OK) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err));
        }
        err = twai_node_transmit_wait_all_done(node, 500);
        if (err == ESP_OK) {
            if (identifier_extended_seen) {
                ESP_LOGI("ACV", "CAN bus discovered extended identifier, supported pids: %#b", supported_pids);
                return;
            }
        } else if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGW("ACV", "CAN bus timed out attemping to discover extended identifier: %s", esp_err_to_name(err));
        } else if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }    
}

void query_pid(const uint8_t *query, size_t size) {
    uint32_t id;
    uint32_t ide;

    if (identifier_extended_seen) {
        id = FUNCTIONAL_ADDRESS_EXTENDED;
        ide = true;
    } else if (identifier_base_seen) {
        id = FUNCTIONAL_ADDRESS_BASE;
        ide = false;
    } else {
        // TODO: use assert?
        return;
    }

    twai_frame_t msg = {
        .header.id = id,
        .header.ide = ide,
        .buffer = (uint8_t *)query,
        .buffer_len = size,
    };
    esp_err_t err = twai_node_transmit(node, &msg, 0);
    if (err != ESP_OK) {
        ESP_LOGE("ACV", "transmit failed: %s", esp_err_to_name(err));
    }
}

void twai_tx_task(void *arg) {
    discover_identifier();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        query_pid(PID_RPM, sizeof(PID_RPM));

        vTaskDelay(pdMS_TO_TICKS(10));
        query_pid(PID_COOLANT_TEMP, sizeof(PID_COOLANT_TEMP));

        vTaskDelay(pdMS_TO_TICKS(10));
        query_pid(PID_SPEED, sizeof(PID_SPEED));

        vTaskDelay(pdMS_TO_TICKS(10));
        query_pid(PID_INTAKE_AIR_TEMP, sizeof(PID_INTAKE_AIR_TEMP));

        vTaskDelay(pdMS_TO_TICKS(10));
        query_pid(PID_THROTTLE_POSITION, sizeof(PID_THROTTLE_POSITION));
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
