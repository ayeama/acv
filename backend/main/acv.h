#ifndef ACV_H
#define ACV_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool updated;
    int8_t coolant_temp;
    uint16_t rpm;
    int8_t speed;
    int8_t intake_air_temp;
    float throttle_position;
} acv_msg_t;

extern acv_msg_t acv_msg;

void acv_msg_coolant_temp(int8_t coolant_temp);
void acv_msg_rpm(uint16_t rpm);
void acv_msg_speed(int8_t speed);
void acv_msg_intake_air_temp(int8_t intake_air_temp);
void acv_msg_throttle_position(float throttle_position);

void acv_msg_string(char *buf, size_t size);

void initialize_acv();

#endif // ACV_H
