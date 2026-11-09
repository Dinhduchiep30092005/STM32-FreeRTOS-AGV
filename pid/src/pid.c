#include "pid.h"

void pid_init(PID_t *p, float kp, float ki, float kd, float out_min, float out_max, float int_limit) {
    p->Kp = kp;
    p->Ki = ki;
    p->Kd = kd;
    p->out_min = out_min;
    p->out_max = out_max;
    p->int_limit = int_limit;
    pid_reset(p);
}

void pid_reset(PID_t *p) {
    p->integral = 0.0f;
    p->prev_error = 0.0f;
}

float pid_update(PID_t *p, float setpoint, float measured, float dt) {
    if (dt <= 0.0f) return 0.0f;

    float error = setpoint - measured;

    // 1. Khâu P
    float p_out = p->Kp * error;

    // 2. Khâu I kèm Anti-windup
    p->integral += error * dt;
    if (p->integral > p->int_limit)  p->integral = p->int_limit;
    if (p->integral < -p->int_limit) p->integral = -p->int_limit;
    float i_out = p->Ki * p->integral;

    // 3. Khâu D
    float derivative = (error - p->prev_error) / dt;
    float d_out = p->Kd * derivative;
    p->prev_error = error;

    // 4. Tổng đầu ra và kẹp bão hòa
    float output = p_out + i_out + d_out;
    if (output > p->out_max) output = p->out_max;
    if (output < p->out_min) output = p->out_min;

    return output;
}