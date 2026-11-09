#include "agv_fc51.h"
AGV_FC51Sample AGV_FC51_Read(void)
{
    AGV_FC51Sample s;
    s.left_detected = HAL_GPIO_ReadPin(AGV_FC_L_PORT, AGV_FC_L_PIN) == AGV_FC_ACTIVE;
    s.right_detected = HAL_GPIO_ReadPin(AGV_FC_R_PORT, AGV_FC_R_PIN) == AGV_FC_ACTIVE;
    return s;
}
