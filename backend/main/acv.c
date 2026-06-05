#include "freertos/FreeRTOS.h"

#include <acv.h>

acv_msg_t acv_msg = {0};

void acv_msg_coolant_temp(int8_t coolant_temp) {
    if (coolant_temp != acv_msg.coolant_temp) {
        acv_msg.coolant_temp = coolant_temp;
        acv_msg.updated = true;
    }
}

void acv_msg_rpm(float rpm) {
    if (rpm != acv_msg.rpm) {
        acv_msg.rpm = rpm;
        acv_msg.updated = true;
    }
}

void acv_msg_speed(int8_t speed) {
    if (speed != acv_msg.speed) {
        acv_msg.speed = speed;
        acv_msg.updated = true;
    }
}

void acv_msg_intake_air_temp(int8_t intake_air_temp) {
    if (intake_air_temp != acv_msg.intake_air_temp) {
        acv_msg.intake_air_temp = intake_air_temp;
        acv_msg.updated = true;
    }
}

void acv_msg_throttle_position(float throttle_position) {
    if (throttle_position != acv_msg.throttle_position) {
        acv_msg.throttle_position = throttle_position;
        acv_msg.updated = true;
    }
}

void acv_msg_string(char *buf, size_t size) {
    snprintf(
        buf,
        size,
        "{\"coolant_temp\":%d,\"rpm\":%f,\"speed\":%d,\"intake_air_temp\":%d,\"throttle_position\":%f}",
        acv_msg.coolant_temp,
        acv_msg.rpm,
        acv_msg.speed,
        acv_msg.intake_air_temp,
        acv_msg.throttle_position
    );
}

void initialize_acv() {
}
