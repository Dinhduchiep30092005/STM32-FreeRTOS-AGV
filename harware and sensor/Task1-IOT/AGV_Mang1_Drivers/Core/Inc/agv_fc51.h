#ifndef AGV_FC51_H
#define AGV_FC51_H
#include "agv_sensor_board.h"
typedef struct { bool left_detected; bool right_detected; } AGV_FC51Sample;
AGV_FC51Sample AGV_FC51_Read(void);
#endif
