#include "agv_sonar.h"
static TIM_HandleTypeDef *us_timer = NULL;
static volatile AGV_SonarState state=AGV_SONAR_IDLE;
static volatile uint16_t rise_ticks=0, pulse_us=0;
static uint32_t trigger_ms=0;
void AGV_Sonar_Init(TIM_HandleTypeDef *microsecond_timer)
{
    us_timer=microsecond_timer;
    state=AGV_SONAR_IDLE;
    rise_ticks=0; pulse_us=0; trigger_ms=0;
    HAL_GPIO_WritePin(AGV_TRIG_PORT,AGV_TRIG_PIN,GPIO_PIN_RESET);
}
bool AGV_Sonar_Trigger(void)
{
    if (us_timer == NULL) return false;
    if (state==AGV_SONAR_WAIT_RISE || state==AGV_SONAR_WAIT_FALL) return false;
    /* Do not trigger while echo is still high. */
    if (HAL_GPIO_ReadPin(AGV_ECHO_PORT,AGV_ECHO_PIN)==GPIO_PIN_SET) return false;
    uint32_t primask=__get_PRIMASK(); __disable_irq();
    pulse_us=0; state=AGV_SONAR_WAIT_RISE;
    if (!primask) __enable_irq();
    trigger_ms=HAL_GetTick();
    HAL_GPIO_WritePin(AGV_TRIG_PORT,AGV_TRIG_PIN,GPIO_PIN_SET);
    uint16_t start=(uint16_t)__HAL_TIM_GET_COUNTER(us_timer);
    while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(us_timer)-start)<10U) { /* 10us */ }
    HAL_GPIO_WritePin(AGV_TRIG_PORT,AGV_TRIG_PIN,GPIO_PIN_RESET);
    return true;
}
void AGV_Sonar_OnEXTI(uint16_t pin)
{
    if (pin!=AGV_ECHO_PIN || us_timer==NULL) return;
    GPIO_PinState level=HAL_GPIO_ReadPin(AGV_ECHO_PORT,AGV_ECHO_PIN);
    if (level==GPIO_PIN_SET && state==AGV_SONAR_WAIT_RISE) {
        rise_ticks=(uint16_t)__HAL_TIM_GET_COUNTER(us_timer);
        state=AGV_SONAR_WAIT_FALL;
    } else if (level==GPIO_PIN_RESET && state==AGV_SONAR_WAIT_FALL) {
        uint16_t end_ticks=(uint16_t)__HAL_TIM_GET_COUNTER(us_timer);
        pulse_us=(uint16_t)(end_ticks-rise_ticks); /* modulo 65536; echo < 35ms */
        state=AGV_SONAR_READY;
    }
}
AGV_SonarSample AGV_Sonar_Poll(void)
{
    AGV_SonarSample s={0};
    uint32_t primask=__get_PRIMASK(); __disable_irq();
    s.state=state;
    s.echo_us=pulse_us;
    if (!primask) __enable_irq();
    if ((s.state==AGV_SONAR_WAIT_RISE || s.state==AGV_SONAR_WAIT_FALL) &&
        (uint32_t)(HAL_GetTick()-trigger_ms)>AGV_SONAR_TIMEOUT_MS) {
        primask=__get_PRIMASK(); __disable_irq();
        if (state==AGV_SONAR_WAIT_RISE || state==AGV_SONAR_WAIT_FALL) state=AGV_SONAR_TIMEOUT;
        s.state=state;
        if (!primask) __enable_irq();
    }
    s.valid=(s.state==AGV_SONAR_READY && s.echo_us>=100U && s.echo_us<=30000U);
    s.distance_cm=s.valid?((float)s.echo_us/58.0f):0.0f;
    return s;
}
