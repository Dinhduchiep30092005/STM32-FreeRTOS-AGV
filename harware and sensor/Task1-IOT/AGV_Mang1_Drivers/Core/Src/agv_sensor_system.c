#include "agv_sensor_system.h"
void AGV_Sensors_Init(TIM_HandleTypeDef *htim2_us, TIM_HandleTypeDef *htim1_pwm,
                      float encoder_counts_per_wheel_rev)
{
    HAL_TIM_Base_Start(htim2_us);
    AGV_Sonar_Init(htim2_us);
    AGV_Encoder_Init(encoder_counts_per_wheel_rev);
    AGV_Servo_Start(htim1_pwm);
    AGV_Servo_SetAngle(htim1_pwm,90U);
}
void AGV_Sensors_OnEXTI(uint16_t gpio_pin)
{
    AGV_Encoder_OnEXTI(gpio_pin);
    AGV_Sonar_OnEXTI(gpio_pin);
}
