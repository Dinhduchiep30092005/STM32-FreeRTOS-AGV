#ifndef AGV_SONAR_H
#define AGV_SONAR_H
#include "agv_sensor_board.h"
typedef enum { AGV_SONAR_IDLE, AGV_SONAR_WAIT_RISE,
               AGV_SONAR_WAIT_FALL, AGV_SONAR_READY, AGV_SONAR_TIMEOUT } AGV_SonarState;
typedef struct { AGV_SonarState state; float distance_cm; uint16_t echo_us; bool valid; } AGV_SonarSample;
void AGV_Sonar_Init(TIM_HandleTypeDef *microsecond_timer);
bool AGV_Sonar_Trigger(void); /* call from task at most ~every 60ms; 10us busy wait */
void AGV_Sonar_OnEXTI(uint16_t pin);
AGV_SonarSample AGV_Sonar_Poll(void); /* nonblocking */
#endif
