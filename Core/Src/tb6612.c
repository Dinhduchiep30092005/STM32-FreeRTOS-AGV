#include "tb6612.h"
#include <stdlib.h>
#include "main.h" // Thư viện chân GPIO sinh ra từ STM32CubeMX

extern TIM_HandleTypeDef htim3; // TIM3: PWMA = PA6 (CH1), PWMB = PA7 (CH2); TIM1 dành cho servo

#define MOTOR_TIM       htim3
#define CHANNEL_L       TIM_CHANNEL_1
#define CHANNEL_R       TIM_CHANNEL_2

void tb6612_init(void) {
    HAL_TIM_PWM_Start(&MOTOR_TIM, CHANNEL_L);
    HAL_TIM_PWM_Start(&MOTOR_TIM, CHANNEL_R);
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_SET);
    tb6612_stop();
}

void tb6612_set_motor(int16_t pwmL, int16_t pwmR) {
    // Động cơ Trái (Kênh A)
    if (pwmL > 0) {
        HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
    } else if (pwmL < 0) {
        HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
    }
    __HAL_TIM_SET_COMPARE(&MOTOR_TIM, CHANNEL_L, abs(pwmL));

    // Động cơ Phải (Kênh B)
    if (pwmR > 0) {
        HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
    } else if (pwmR < 0) {
        HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
    }
    __HAL_TIM_SET_COMPARE(&MOTOR_TIM, CHANNEL_R, abs(pwmR));
}

void tb6612_stop(void) {
    HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&MOTOR_TIM, CHANNEL_L, 0);
    __HAL_TIM_SET_COMPARE(&MOTOR_TIM, CHANNEL_R, 0);
}