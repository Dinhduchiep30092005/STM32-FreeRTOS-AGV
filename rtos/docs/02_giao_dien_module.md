# 02. Ghép code của các mảng

Cách phần RTOS dùng driver cảm biến (Mảng 1) và bộ điều khiển (Mảng 2): quy ước chung, kiểu dữ liệu trao đổi giữa các task, cách chuyển đổi dữ liệu, và **các sửa đổi ở code Mảng 2** (việc 1–7 đã xong, việc 8 chưa làm).

[← Về README](README.md)

---

## 1. Quy ước chung cả nhóm

| Đại lượng | Quy ước | Nguồn |
|---|---|---|
| Sai số line `error` | float −2…+2. **Âm = line bên TRÁI xe, dương = line bên PHẢI xe** | `AGV_Line_Read()` (trọng số mắt trái nhất = −2) |
| Hướng rẽ | `error > 0` → xe phải rẽ phải → **bánh trái nhanh hơn**: `v_L = V_base + u`, `v_R = V_base − u` với `u > 0` | Thống nhất nhóm |
| Bit line | **Bit 4 = mắt trái nhất (S0)**, bit 0 = mắt phải nhất (S4). 1 = thấy line | `AGV_Line_Read()` |
| Mất line | `valid = false`. `error = 0` lúc này **không** có nghĩa là ở giữa line | Mảng 1 |
| Tốc độ bánh | rpm **không dấu** (FC-03 một kênh không biết chiều quay) | Mảng 1 |
| PWM | `int16_t` −1000…+1000. Dương = tiến | `control.c`, `tb6612.c` |
| Khoảng cách | `distance_cm` float, kèm `valid` | `AGV_Sonar_Poll()` |
| Góc servo | 0…180°, **90° = nhìn thẳng** | `AGV_Servo_SetAngle()` |
| `dt` của PID | giây, 0,01 cho chu kỳ 10 ms | |

**Kiểm tra dấu ngay khi lắp xe:** đặt vạch đen lệch sang **phải** xe, telemetry phải ra `err` **dương**. Nếu âm thì thứ tự 5 dây dò line đang cắm ngược, sửa dây chứ không sửa code.

Cực tính dò line: nhiều module TCRT5000 ra **mức 1 khi ở trên vạch đen** (vạch đen hấp thụ hồng ngoại). `AGV_LINE_ACTIVE` trong `agv_sensor_board.h` đang để `GPIO_PIN_RESET`; Mảng 1 test rồi đổi nếu cần.

---

## 2. Kiểu dữ liệu trao đổi giữa các task

Khai báo trong `app_rtos.h` (Mảng 3), dùng lại struct của Mảng 1:

```c
#include "agv_sensor_system.h"   /* Mảng 1 */
#include "control.h"             /* Mảng 2 */
#include "tb6612.h"              /* Mảng 2 */

/* SensorTask -> qSensor */
typedef struct {
    AGV_LineSample line;     /* bits, valid, error (float)          */
    float    rpmL, rpmR;     /* rpm đã làm mượt (cửa sổ 100 ms)    */
    uint32_t tick;
} SensorData_t;

/* SonarTask -> qRange */
typedef struct {
    AGV_SonarSample sonar;   /* state, distance_cm, echo_us, valid  */
    AGV_FC51Sample  fc;      /* left_detected, right_detected       */
    uint8_t  servoDeg;       /* góc lúc đo                          */
    uint32_t tick;
} RangeData_t;

/* ControlTask -> qCtrl (để hiển thị, gửi log) */
typedef struct {
    int16_t  pwmL, pwmR;
    int8_t   errSent;        /* err đã truyền vào control_line_step */
    uint32_t tick;
} CtrlOut_t;

/* Tham số chỉnh qua Bluetooth, bảo vệ bằng mtxParams */
typedef struct {
    float    kpLine, kdLine;     /* vòng ngoài bám line                */
    float    kpSpd,  kiSpd;      /* vòng trong tốc độ bánh             */
    float    kpDist;             /* PID khoảng cách                    */
    float    vBase;              /* tốc độ cơ bản (rpm)                */
    uint32_t version;            /* tăng mỗi lần đổi, ControlTask so   */
} Params_t;
```

Giá trị mặc định lấy đúng từ `control_init()` của Mảng 2: `kpLine 45`, `kdLine 3.5`, `kpSpd 7.5`, `kiSpd 1.2`, `kpDist 8`, `vBase 120`.

---

## 3. Chuyển đổi dữ liệu giữa các mảng

### 3.1. Line: float → `int8_t` của `control_line_step`

`control_line_step()` nhận `int8_t err` và quy ước `99` / `−99` là mất line. ControlTask chuyển như sau:

```c
#define LINE_LOST_CODE  99

static int8_t line_to_err(const AGV_LineSample *l)
{
    if (!l->valid) return LINE_LOST_CODE;
    float e = l->error;                              /* -2.0 .. +2.0 */
    return (int8_t)(e >= 0.0f ? e + 0.5f : e - 0.5f);   /* làm tròn  */
}
```

Làm tròn sẽ mất độ mịn ±0,5 và ±1,5 của trung bình trọng số. Mảng 2 nên đổi tham số sang `float err` + `bool valid` (mục 4, việc 8).

### 3.2. Encoder: làm mượt rpm trong SensorTask

`AGV_Encoder_Sample()` tính rpm từ **số xung đếm được** giữa hai lần gọi. Với đĩa 20 lỗ, ở khoảng 200 rpm chỉ có 0–1 xung mỗi 10 ms, rpm nhảy giữa 0 và khoảng 300. Tạm thời SensorTask cộng `left_delta`, `right_delta`, `dt_ms` của **10 mẫu gần nhất** (cửa sổ 100 ms) rồi mới tính rpm:

```
rpm = Σdelta × 60000 / (CPWR × Σdt_ms)
```

Cửa sổ 100 ms làm rpm trễ khoảng 50 ms so với thực tế. PID vòng trong vẫn chạy được nhưng phải đặt hệ số mềm: mỗi xung trong cửa sổ tương ứng 30 rpm (20 xung/vòng), nên `control_init()` đặt Kd vòng trong = 0 để tránh gai khoảng 300 PWM, còn Kp chỉnh trên xe bằng lệnh `KP2`. **Cách tốt hơn** là Mảng 1 bổ sung đo chu kỳ giữa 2 xung (mục 5).

`AGV_Encoder_Sample()` có trạng thái bên trong (`prev_*`) nên **chỉ SensorTask được gọi**.

### 3.3. Sonar: chỉ gọi PID khoảng cách khi dữ liệu hợp lệ

Khi `sonar.valid == false`, `distance_cm = 0`. Nếu truyền 0 vào `control_follow_step()`, PID khoảng cách sẽ hiểu là vật ở 0 cm. Vì vậy ControlTask **tự dừng xe khi sonar không hợp lệ**, không gọi `control_follow_step()`.

---

## 4. Các sửa đổi ở Mảng 2 trong `src/pid/src`

> **Trạng thái:** Việc 1–7 đã sửa trong code ngày 9/11, Việc 8 chưa làm. Nội dung bên dưới giữ lại để đối chiếu.

Đọc code thực tế ngày 9/11. Ba việc đầu **bắt buộc** trước khi đặt xe xuống sàn.

### Việc 1 (bắt buộc). Dấu vòng ngoài đang ngược: xe rẽ **ra xa** line

```c
float u_steer = pid_update(&pid_line, 0.0f, (float)err, dt);   /* error = 0 - err = -err */
v_target_L = V_BASE_DEFAULT + u_steer;
```

Với `err = +2` (line bên phải): `u = 45 × (−2) = −90` → `v_L = 30`, `v_R = 210` → bánh phải nhanh hơn → xe rẽ **trái**, đi ra xa line. `main.c` test của Mảng 2 cũng cho ra PWM_R > PWM_L ở `err = +2`.

**Sửa** (đảo setpoint và measured để `u` cùng dấu với `err`):

```c
float u_steer = pid_update(&pid_line, (float)err, 0.0f, dt);   /* u > 0 khi err > 0 */
v_target_L = V_BASE_DEFAULT + u_steer;                          /* bánh trái nhanh  */
v_target_R = V_BASE_DEFAULT - u_steer;
```

### Việc 2 (bắt buộc). Dấu PID khoảng cách đang ngược: xe **lùi** khi vật ở xa

```c
float v_base_follow = pid_update(&pid_dist, 20.0f, dist_cm, dt);   /* error = 20 - dist */
```

Vật ở 30 cm → `error = −10` → V_base âm → xe lùi xa vật thay vì tiến lại gần.

**Sửa:**

```c
float v_base_follow = pid_update(&pid_dist, dist_cm, 20.0f, dt);   /* > 0 khi vật xa hơn 20 cm */
if (v_base_follow < 0.0f) v_base_follow = 0.0f;                    /* không lùi, xem việc 3   */
```

### Việc 3 (bắt buộc). Tốc độ mục tiêu âm + rpm không dấu → động cơ chạy hết ga

FC-03 một kênh luôn trả rpm ≥ 0. Khi đặt `v_target = −60` (tìm line khi mất vạch) hoặc V_base âm:

- Bánh quay lùi, rpm đo được vẫn là số dương, ví dụ 60.
- `error = −60 − 60 = −120`, PID thấy "càng quay càng sai" nên đẩy PWM âm mạnh hơn.
- Tích phân dồn tới giới hạn, **bánh quay lùi hết tốc độ** và không bao giờ ổn định.

**Sửa** (cách đơn giản, không cần biết chiều quay): **không bao giờ đặt tốc độ mục tiêu âm**. Khi mất line thì quay tại chỗ bằng cách cho một bánh đứng yên, một bánh chạy:

```c
if (err == 99 || err == -99) {
    if (last_valid_err < 0) { v_target_L = 0.0f;               v_target_R = LOST_LINE_SEARCH_V; }
    else                    { v_target_L = LOST_LINE_SEARCH_V; v_target_R = 0.0f; }
}
/* ... sau khi tính v_target_L, v_target_R ở mọi nhánh: */
if (v_target_L < 0.0f) v_target_L = 0.0f;
if (v_target_R < 0.0f) v_target_R = 0.0f;
```

Đổi giới hạn đầu ra PID tốc độ thành `0 … MAX_PWM` thay vì `−MAX_PWM … MAX_PWM` (vòng trong chỉ điều khiển chạy tiến). Muốn cho xe lùi thật thì phải tự suy chiều quay từ dấu PWM đã xuất, phức tạp hơn và không cần cho bài này.

### Việc 4. Đổi timer PWM sang TIM3

```c
extern TIM_HandleTypeDef htim1;   /* SAI: TIM1 là servo (PA8) */
#define MOTOR_TIM htim1
```

Theo sơ đồ chân chung: PWMA = PA6 = **TIM3_CH1**, PWMB = PA7 = **TIM3_CH2**. Sửa thành `htim3`. TIM3 cấu hình ARR = 999 (bước 5 của [`01`](01_cau_hinh_cubemx.md)), khớp `MAX_PWM = 1000`.

### Việc 5. Đổi tên file `tb6612..c` → `tb6612.c`

Tên có hai dấu chấm, CubeIDE có thể không nhận là file C.

### Việc 6. `main.c` là chương trình test trên máy tính

Đổi tên thành `test_control_pc.c` và để ngoài project STM32, tránh trùng `main()` với file CubeMX sinh.

### Việc 7. Thêm hàm đổi hệ số khi đang chạy

Hệ số PID đang cố định trong `control_init()`, nên lệnh chỉnh qua Bluetooth chưa có tác dụng. Thêm vào `control.c`:

```c
static float v_base = V_BASE_DEFAULT;      /* thay cho hằng số trong control_line_step */

void control_set_gains(float kp_line, float kd_line,
                       float kp_spd,  float ki_spd,
                       float kp_dist, float vbase)
{
    pid_line.Kp = kp_line;  pid_line.Kd = kd_line;
    pid_speed_L.Kp = pid_speed_R.Kp = kp_spd;
    pid_speed_L.Ki = pid_speed_R.Ki = ki_spd;
    pid_dist.Kp = kp_dist;
    v_base = vbase;
    /* không reset tích phân để xe không giật khi đổi hệ số */
}
```

Chỉ ControlTask gọi hàm này (cùng task với `control_line_step`), nên không cần khóa bên trong `control.c`. Hàm đã có trong `control.c`, `app_config.h` đặt `CONTROL_HAS_SET_GAINS 1`.

### Việc 8 (nên làm). Cải thiện chất lượng điều khiển

- **Nhận `float err` + `bool valid`** thay cho `int8_t` + mã 99: giữ được độ mịn ±0,5 của dò line.
- **Vòng ngoài chạy 20 ms** như sơ đồ pipeline (hiện chạy mỗi lần gọi = 10 ms): thêm biến đếm, chỉ cập nhật `u_steer` mỗi 2 lần gọi.
- **Khâu D vòng ngoài:** `err` nhảy bậc nên `Kd × Δerr / dt` có gai lớn (Δerr = 1 → 3,5 / 0,01 = 350, vượt giới hạn ±100). Lọc thông thấp đạo hàm hoặc giảm Kd.
- **Ngưỡng dừng 15 cm trong `control_follow_step`** trùng với ngưỡng RTOS. Giữ lại cũng được (hai lớp an toàn), nhưng ghi rõ trong báo cáo.

---

## 5. Đề xuất cho Mảng 1: đo encoder theo chu kỳ xung

Thêm vào `agv_encoder.c`, tận dụng TIM2 đang đếm µs:

```c
static TIM_HandleTypeDef *enc_tim;           /* gán = htim2 trong AGV_Encoder_Init */
static volatile uint16_t last_us[2];
static volatile uint32_t period_us[2], last_ms[2];

/* gọi thêm trong AGV_Encoder_OnEXTI(pin), side = 0 trái, 1 phải */
static void enc_period_isr(uint8_t side)
{
    uint16_t now_us = (uint16_t)__HAL_TIM_GET_COUNTER(enc_tim);
    uint32_t now_ms = HAL_GetTick();
    if (now_ms - last_ms[side] > 60U) {           /* xung đầu sau khi đứng yên */
        period_us[side] = 0;
    } else {
        uint16_t p = (uint16_t)(now_us - last_us[side]);
        if (p < 1000U) return;                    /* < 1 ms: nhiễu LM393, bỏ */
        period_us[side] = p;
    }
    last_us[side] = now_us;
    last_ms[side] = now_ms;
}

float AGV_Encoder_GetRpmPeriod(uint8_t side)
{
    uint32_t primask = __get_PRIMASK(); __disable_irq();
    uint32_t p = period_us[side], t = last_ms[side];
    if (!primask) __enable_irq();
    if (p == 0U || HAL_GetTick() - t > 150U) return 0.0f;
    return 60.0e6f / ((float)p * cpwr);
}
```

Độ phân giải 1 µs, rpm mượt ngay ở chu kỳ 10 ms, không cần cửa sổ 100 ms. Khi có hàm này, SensorTask đổi sang gọi `AGV_Encoder_GetRpmPeriod()`.

---

## 6. Checklist trước khi ghép

- [x] Mảng 2 đã sửa việc 1–7.
- [ ] Mảng 1 đã test cực tính dò line (`AGV_LINE_ACTIVE`), FC-51 (`AGV_FC_ACTIVE`), và hiệu chuẩn `CPWR`.
- [ ] Không file nào ngoài `app_rtos.c` định nghĩa `HAL_GPIO_EXTI_Callback`.
- [ ] Không file nào ngoài `app_rtos.c` gọi `tb6612_set_motor()` / `tb6612_stop()` (trừ hook lỗi).
- [ ] Không có `HAL_Delay()` trong code được gọi từ task (trừ `ssd1306_Init()` của thư viện OLED, chạy một lần ở UiTask ưu tiên thấp nhất).
- [ ] Line lệch phải → telemetry `err` dương → bánh trái nhanh hơn (kiểm tra khi kê xe lên).
