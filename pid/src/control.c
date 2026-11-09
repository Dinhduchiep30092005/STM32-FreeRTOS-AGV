#include "control.h"

#define V_BASE_DEFAULT     120.0f   // Vận tốc đặt thẳng cơ bản (rpm)
#define MAX_PWM            1000     // Giới hạn độ rộng xung PWM (0 - 1000)
#define LOST_LINE_SEARCH_V 60.0f    // Tốc độ xoay tại chỗ tìm line khi mất vạch

// Khai báo 4 bộ PID nội bộ
static PID_t pid_line;
static PID_t pid_dist;
static PID_t pid_speed_L;
static PID_t pid_speed_R;

static int8_t last_valid_err = 0;

void control_init(void) {
    // PID Line (vòng ngoài): err [-2..+2] -> chênh lệch tốc độ [-100..+100 rpm]
    pid_init(&pid_line, 45.0f, 0.0f, 3.5f, -100.0f, 100.0f, 20.0f);
    // PID Khoảng cách: khoảng cách -> V_base [-150..+150 rpm]
    pid_init(&pid_dist, 8.0f, 0.2f, 1.0f, -150.0f, 150.0f, 50.0f);
    // PID Tốc độ 2 bánh (vòng trong): sai số rpm -> xung PWM [-1000..+1000]
    pid_init(&pid_speed_L, 7.5f, 1.2f, 0.1f, -MAX_PWM, MAX_PWM, 300.0f);
    pid_init(&pid_speed_R, 7.5f, 1.2f, 0.1f, -MAX_PWM, MAX_PWM, 300.0f);
}

void control_reset_all(void) {
    pid_reset(&pid_line);
    pid_reset(&pid_dist);
    pid_reset(&pid_speed_L);
    pid_reset(&pid_speed_R);
    last_valid_err = 0;
}

void control_line_step(int8_t err, float rpmL, float rpmR,
                       float dt, int16_t *pwmL, int16_t *pwmR) {
    float v_target_L = 0.0f;
    float v_target_R = 0.0f;

    // Xử lý khi mất line (err = 99 hoặc -99)
    if (err == 99 || err == -99) {
        if (last_valid_err < 0) {
            v_target_L = -LOST_LINE_SEARCH_V;
            v_target_R =  LOST_LINE_SEARCH_V;
        } else {
            v_target_L =  LOST_LINE_SEARCH_V;
            v_target_R = -LOST_LINE_SEARCH_V;
        }
    } else {
        last_valid_err = err;
        float u_steer = pid_update(&pid_line, 0.0f, (float)err, dt);
        v_target_L = V_BASE_DEFAULT + u_steer;
        v_target_R = V_BASE_DEFAULT - u_steer;
    }

    // Vòng trong: tính giá trị PWM cấp cho 2 bánh
    *pwmL = (int16_t)pid_update(&pid_speed_L, v_target_L, rpmL, dt);
    *pwmR = (int16_t)pid_update(&pid_speed_R, v_target_R, rpmR, dt);
}

void control_follow_step(float dist_cm, uint8_t fcL, uint8_t fcR,
                         float rpmL, float rpmR, float dt,
                         int16_t *pwmL, int16_t *pwmR) {
    // Dừng an toàn khi vật cản quá gần (< 15cm)
    if (dist_cm < 15.0f && dist_cm > 0.0f) {
        control_reset_all();
        *pwmL = 0;
        *pwmR = 0;
        return;
    }

    float v_base_follow = pid_update(&pid_dist, 20.0f, dist_cm, dt);
    float u_turn = 0.0f;
    if (fcL == 1 && fcR == 0)      u_turn = -35.0f;
    else if (fcL == 0 && fcR == 1) u_turn =  35.0f;

    float v_target_L = v_base_follow + u_turn;
    float v_target_R = v_base_follow - u_turn;

    *pwmL = (int16_t)pid_update(&pid_speed_L, v_target_L, rpmL, dt);
    *pwmR = (int16_t)pid_update(&pid_speed_R, v_target_R, rpmR, dt);
}