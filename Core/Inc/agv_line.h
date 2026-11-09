#ifndef AGV_LINE_H
#define AGV_LINE_H
#include "agv_sensor_board.h"
typedef struct {
    uint8_t bits; /* bit 4 = leftmost S0, bit 0 = rightmost S4; 1 means sees line */
    bool valid;
    float error; /* weighted mean -2 .. +2 */
} AGV_LineSample;
AGV_LineSample AGV_Line_Read(void);
#endif
