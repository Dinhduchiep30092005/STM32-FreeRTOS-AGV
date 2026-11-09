#ifndef PID_H
#define PID_H

#include <stdint.h>

typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float integral;
    float prev_error;
    float out_min;
    float out_max;
    float int_limit;
} PID_t;

void  pid_init(PID_t *p, float kp, float ki, float kd, float out_min, float out_max, float int_limit);
void  pid_reset(PID_t *p);
float pid_update(PID_t *p, float setpoint, float measured, float dt);

#endif // PID_H