#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include "pid.h"

void control_init(void);
void control_reset_all(void);

// Đổi hệ số khi đang chạy, chỉ ControlTask được gọi
void control_set_gains(float kp_line, float kd_line,
                       float kp_spd,  float ki_spd,
                       float kp_dist, float vbase);

// 2 hàm nhóm trưởng yêu cầu ControlTask gọi
void control_line_step(int8_t err, float rpmL, float rpmR,
                       float dt, int16_t *pwmL, int16_t *pwmR);

void control_follow_step(float dist_cm, uint8_t fcL, uint8_t fcR,
                         float rpmL, float rpmR, float dt,
                         int16_t *pwmL, int16_t *pwmR);

#endif // CONTROL_H