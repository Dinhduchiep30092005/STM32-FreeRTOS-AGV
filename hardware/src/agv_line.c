#include "agv_line.h"
AGV_LineSample AGV_Line_Read(void)
{
    const uint16_t pins[5] = {AGV_LINE_0, AGV_LINE_1, AGV_LINE_2, AGV_LINE_3, AGV_LINE_4};
    const int8_t weights[5] = {-2,-1,0,1,2};
    AGV_LineSample out = {0};
    int32_t weighted = 0;
    uint32_t count = 0;
    for (uint32_t i=0; i<5; ++i) {
        bool detected = HAL_GPIO_ReadPin(AGV_LINE_PORT,pins[i]) == AGV_LINE_ACTIVE;
        if (detected) {
            out.bits |= (uint8_t)(1U << (4U-i));
            weighted += weights[i];
            count++;
        }
    }
    out.valid = (count != 0U);
    out.error = out.valid ? ((float)weighted / (float)count) : 0.0f;
    /* 00000 = lost line; 11111 may be intersection - application interprets. */
    return out;
}
