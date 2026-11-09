#ifndef AGV_ENCODER_H
#define AGV_ENCODER_H
#include "agv_sensor_board.h"
typedef struct {
    uint32_t left_count;
    uint32_t right_count;
    uint32_t left_delta;
    uint32_t right_delta;
    float left_rpm_magnitude;
    float right_rpm_magnitude;
    uint32_t dt_ms;
    bool valid;
} AGV_EncoderSample;
void AGV_Encoder_Init(float counts_per_wheel_rev);
void AGV_Encoder_OnEXTI(uint16_t gpio_pin);
AGV_EncoderSample AGV_Encoder_Sample(void);
#endif
