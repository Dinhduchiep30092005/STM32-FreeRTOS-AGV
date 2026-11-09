#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ----- Chu kỳ (ms) ----- */
#define CONTROL_PERIOD_MS      10
#define SENSOR_PERIOD_MS       10
#define SONAR_PERIOD_MS        60
#define MODE_WAIT_MS           20
#define COMM_TELEMETRY_MS     100
#define UI_PERIOD_MS          100   /* OLED vẽ lại mỗi 2 chu kỳ */

/* ----- Ưu tiên (số lớn = cao) ----- */
#define PRIO_CONTROL            5
#define PRIO_SENSOR             4
#define PRIO_SONAR              3
#define PRIO_MODE               3
#define PRIO_COMM               2
#define PRIO_UI                 1

/* ----- Stack (word) ----- */
#define STACK_CONTROL         256
#define STACK_SENSOR          160
#define STACK_SONAR           192
#define STACK_MODE            128
#define STACK_COMM            320
#define STACK_UI              320

/* ----- Encoder ----- */
#define ENC_CPWR             20.0f  /* count / vòng bánh: Mảng 1 hiệu chuẩn lại */
#define ENC_WIN                10   /* cửa sổ làm mượt: 10 mẫu x 10 ms = 100 ms */

/* ----- Siêu âm và servo ----- */
#define OBSTACLE_CM_LINE     15.0f
#define OBSTACLE_CM_FOLLOW    8.0f
#define OBSTACLE_CLEAR_CM    20.0f
#define OBSTACLE_CLEAR_COUNT    3
#define SERVO_CENTER           90
#define SERVO_HOLD_CYCLES       5   /* giữ mỗi góc 5 x 60 ms = 300 ms        */
#define SERVO_SETTLE_CYCLES     3   /* chỉ tin kết quả đo sau 180 ms đứng yên */

/* ----- Máy trạng thái ----- */
#define STOP_RESUME_MS       1000
#define LINE_LOST_IDLE_MS    2000
#define SENSOR_STALE_MS        50   /* dữ liệu cũ hơn mức này -> dừng xe */
#define RANGE_STALE_MS        150   /* qRange: hơn 2 chu kỳ sonar (60 ms)   */

/* ----- Tham số mặc định (khớp control_init của Mảng 2) ----- */
#define DEF_KP_LINE          45.0f
#define DEF_KD_LINE           3.5f
#define DEF_KP_SPD            7.5f
#define DEF_KI_SPD            1.2f
#define DEF_KP_DIST           8.0f
#define DEF_VBASE           120.0f

/* Mảng 2 đã thêm control_set_gains() (02, mục 4, việc 7) */
#define CONTROL_HAS_SET_GAINS   1

#endif
