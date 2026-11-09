#include <stdio.h>
#include "control.h"

int main() {
    control_init();

    int16_t pwmL = 0, pwmR = 0;
    float rpmL = 0.0f, rpmR = 0.0f;
    float dt = 0.01f; // Chu kỳ 10ms

    printf("=== TEST 1: BAM LINE (Xe lech trai: err = +2) ===\n");
    for (int i = 1; i <= 3; i++) {
        control_line_step(2, rpmL, rpmR, dt, &pwmL, &pwmR);
        printf("Buoc %d: PWM_L = %4d | PWM_R = %4d\n", i, pwmL, pwmR);
        rpmL += pwmL * 0.1f * dt;
        rpmR += pwmR * 0.1f * dt;
    }

    printf("\n=== TEST 2: BAM VAT THE (Khoang cach = 30cm, vat lech phai) ===\n");
    control_follow_step(30.0f, 0, 1, rpmL, rpmR, dt, &pwmL, &pwmR);
    printf("Ket qua: PWM_L = %4d | PWM_R = %4d\n", pwmL, pwmR);

    printf("\n=== TEST 3: CAN AN TOAN (< 15cm) ===\n");
    control_follow_step(12.0f, 0, 0, rpmL, rpmR, dt, &pwmL, &pwmR);
    printf("Ket qua: PWM_L = %4d | PWM_R = %4d (Yeu cau dung khan: PWM = 0)\n", pwmL, pwmR);

    return 0;
}