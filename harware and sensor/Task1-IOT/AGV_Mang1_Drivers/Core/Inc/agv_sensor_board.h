#ifndef AGV_SENSOR_BOARD_H
#define AGV_SENSOR_BOARD_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Pin proposal for STM32F103C8T6 Blue Pill - configure in CubeMX. */
#define AGV_LINE_PORT       GPIOA
#define AGV_LINE_0         GPIO_PIN_0  /* leftmost */
#define AGV_LINE_1         GPIO_PIN_1
#define AGV_LINE_2         GPIO_PIN_2
#define AGV_LINE_3         GPIO_PIN_3
#define AGV_LINE_4         GPIO_PIN_4  /* rightmost */
#define AGV_LINE_ACTIVE    GPIO_PIN_RESET /* calibration may change */

#define AGV_ENCODER_L_PORT GPIOB
#define AGV_ENCODER_L_PIN  GPIO_PIN_6   /* EXTI rising */
#define AGV_ENCODER_R_PORT GPIOB
#define AGV_ENCODER_R_PIN  GPIO_PIN_7   /* EXTI rising */
#define AGV_ENCODER_COUNTS_PER_WHEEL_REV 20.0f /* PLACEHOLDER: measure */

#define AGV_ECHO_PORT      GPIOB
#define AGV_ECHO_PIN       GPIO_PIN_8   /* EXTI both edges; level shift 5V! */
#define AGV_TRIG_PORT      GPIOB
#define AGV_TRIG_PIN       GPIO_PIN_9

#define AGV_FC_L_PORT      GPIOB
#define AGV_FC_L_PIN       GPIO_PIN_0
#define AGV_FC_R_PORT      GPIOB
#define AGV_FC_R_PIN       GPIO_PIN_1
#define AGV_FC_ACTIVE     GPIO_PIN_RESET /* module dependent */

/* TIM2: 1 MHz tick, ARR=65535; TIM1 CH1: 50Hz (1MHz, ARR=19999) */
#define AGV_SERVO_CHANNEL  TIM_CHANNEL_1 /* PA8, TIM1_CH1 */
#define AGV_SERVO_MIN_US   1000U
#define AGV_SERVO_MAX_US   2000U
#define AGV_SONAR_TIMEOUT_MS 35U

#endif
