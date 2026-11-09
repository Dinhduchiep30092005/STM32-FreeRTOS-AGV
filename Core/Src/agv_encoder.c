#include "agv_encoder.h"
static volatile uint32_t encoder_l = 0, encoder_r = 0;
static uint32_t prev_l=0, prev_r=0, prev_ms=0;
static float cpwr = AGV_ENCODER_COUNTS_PER_WHEEL_REV;
void AGV_Encoder_Init(float counts_per_wheel_rev)
{
    if (counts_per_wheel_rev > 0.0f) cpwr=counts_per_wheel_rev;
    uint32_t primask = __get_PRIMASK(); __disable_irq();
    encoder_l=0; encoder_r=0;
    if (!primask) __enable_irq();
    prev_l=0; prev_r=0; prev_ms=HAL_GetTick();
}
void AGV_Encoder_OnEXTI(uint16_t pin)
{
    if (pin == AGV_ENCODER_L_PIN) encoder_l++;
    else if (pin == AGV_ENCODER_R_PIN) encoder_r++;
}
AGV_EncoderSample AGV_Encoder_Sample(void)
{
    AGV_EncoderSample s={0};
    uint32_t primask = __get_PRIMASK(); __disable_irq();
    s.left_count=encoder_l;
    s.right_count=encoder_r;
    if (!primask) __enable_irq();
    uint32_t now=HAL_GetTick();
    s.dt_ms=now-prev_ms; /* unsigned wrap is defined */
    s.left_delta=s.left_count-prev_l;
    s.right_delta=s.right_count-prev_r;
    s.valid=(s.dt_ms>0U && cpwr>0.0f);
    if (s.valid) {
        s.left_rpm_magnitude=(float)s.left_delta*60000.0f/(cpwr*(float)s.dt_ms);
        s.right_rpm_magnitude=(float)s.right_delta*60000.0f/(cpwr*(float)s.dt_ms);
    }
    prev_l=s.left_count; prev_r=s.right_count; prev_ms=now;
    return s;
}
