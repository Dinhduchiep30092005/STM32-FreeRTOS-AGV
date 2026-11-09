#ifndef AGV_SERVO_H
#define AGV_SERVO_H
#include "agv_sensor_board.h"
HAL_StatusTypeDef AGV_Servo_Start(TIM_HandleTypeDef *htim1);
void AGV_Servo_SetAngle(TIM_HandleTypeDef *htim1, uint8_t angle_deg);
#endif
