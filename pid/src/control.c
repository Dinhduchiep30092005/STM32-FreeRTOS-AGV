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
static float v_base = V_BASE_DEFAULT;

void control_init(void) {
    // PID Line (vòng ngoài): err [-2..+2] -> chênh lệch tốc độ [-100..+100 rpm]
    pid_init(&pid_line, 45.0f, 0.0f, 3.5f, -100.0f, 100.0f, 20.0f);
    // PID Khoảng cách: khoảng cách -> V_base [-150..+150 rpm]
    pid_init(&pid_dist, 8.0f, 0.2f, 1.0f, -150.0f, 150.0f, 50.0f);
    // PID Tốc độ 2 bánh (vòng trong): sai số rpm -> xung PWM [0..+1000], chỉ chạy tiến
    pid_init(&pid_speed_L, 7.5f, 1.2f, 0.1f, 0.0f, MAX_PWM, 300.0f);
    pid_init(&pid_speed_R, 7.5f, 1.2f, 0.1f, 0.0f, MAX_PWM, 300.0f);
}

void control_reset_all(void) {
    pid_reset(&pid_line);
    pid_reset(&pid_dist);
    pid_reset(&pid_speed_L);
    pid_reset(&pid_speed_R);
    last_valid_err = 0;
}

// Chỉ ControlTask được gọi. Không reset tích phân để xe không giật khi đổi hệ số.
void control_set_gains(float kp_line, float kd_line,
                       float kp_spd,  float ki_spd,
                       float kp_dist, float vbase) {
    pid_line.Kp = kp_line;
    pid_line.Kd = kd_line;
    pid_speed_L.Kp = kp_spd;
    pid_speed_R.Kp = kp_spd;
    pid_speed_L.Ki = ki_spd;
    pid_speed_R.Ki = ki_spd;
    pid_dist.Kp = kp_dist;
    v_base = vbase;
}

void control_line_step(int8_t err, float rpmL, float rpmR,
                       float dt, int16_t *pwmL, int16_t *pwmR) {
    float v_target_L = 0.0f;
    float v_target_R = 0.0f;

    // Xử lý khi mất line (err = 99 hoặc -99)
    if (err == 99 || err == -99) {
        if (last_valid_err < 0) {
            // Encoder một kênh không biết chiều quay nên không đặt tốc độ âm:
            // một bánh đứng yên, bánh kia chạy để xoay về phía line vừa mất
            v_target_L = 0.0f;
            v_target_R =  LOST_LINE_SEARCH_V;
        } else {
            v_target_L =  LOST_LINE_SEARCH_V;
            v_target_R = 0.0f;
        }
    } else {
        last_valid_err = err;
        // u_steer cùng dấu với err: err > 0 (line bên phải) -> bánh trái nhanh hơn
        float u_steer = pid_update(&pid_line, (float)err, 0.0f, dt);
        v_target_L = v_base + u_steer;
        v_target_R = v_base - u_steer;
    }

    if (v_target_L < 0.0f) v_target_L = 0.0f;
    if (v_target_R < 0.0f) v_target_R = 0.0f;

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

    // Dương khi vật xa hơn 20 cm -> xe tiến lại gần; không lùi vì encoder không biết chiều quay
    float v_base_follow = pid_update(&pid_dist, dist_cm, 20.0f, dt);
    if (v_base_follow < 0.0f) v_base_follow = 0.0f;

    float u_turn = 0.0f;
    if (fcL == 1 && fcR == 0)      u_turn = -35.0f;
    else if (fcL == 0 && fcR == 1) u_turn =  35.0f;

    float v_target_L = v_base_follow + u_turn;
    float v_target_R = v_base_follow - u_turn;
    if (v_target_L < 0.0f) v_target_L = 0.0f;
    if (v_target_R < 0.0f) v_target_R = 0.0f;

    *pwmL = (int16_t)pid_update(&pid_speed_L, v_target_L, rpmL, dt);
    *pwmR = (int16_t)pid_update(&pid_speed_R, v_target_R, rpmR, dt);
}