#include "agv_servo.h"
HAL_StatusTypeDef AGV_Servo_Start(TIM_HandleTypeDef *htim1)
{
    if (htim1 == NULL) return HAL_ERROR;
    return HAL_TIM_PWM_Start(htim1, AGV_SERVO_CHANNEL);
}
void AGV_Servo_SetAngle(TIM_HandleTypeDef *htim1, uint8_t angle_deg)
{
    if (htim1 == NULL) return;
    if (angle_deg > 180) angle_deg = 180;
    uint32_t us=AGV_SERVO_MIN_US +
        ((uint32_t)(AGV_SERVO_MAX_US-AGV_SERVO_MIN_US)*angle_deg)/180U;
    __HAL_TIM_SET_COMPARE(htim1, AGV_SERVO_CHANNEL, us);
}
