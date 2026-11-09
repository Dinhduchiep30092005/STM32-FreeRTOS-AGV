#ifndef AGV_SENSOR_SYSTEM_H
#define AGV_SENSOR_SYSTEM_H
#include "agv_line.h"
#include "agv_encoder.h"
#include "agv_sonar.h"
#include "agv_servo.h"
#include "agv_fc51.h"
typedef struct {
    AGV_LineSample line;
    AGV_EncoderSample encoder;
    AGV_FC51Sample obstacle;
    AGV_SonarSample sonar;
    uint8_t servo_angle_deg;
} AGV_SensorData;
void AGV_Sensors_Init(TIM_HandleTypeDef *htim2_us, TIM_HandleTypeDef *htim1_pwm,
                      float encoder_counts_per_wheel_rev);
/* Call from your one HAL_GPIO_EXTI_Callback in main.c */
void AGV_Sensors_OnEXTI(uint16_t gpio_pin);
#endif
