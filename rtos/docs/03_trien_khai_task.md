# 03. Triển khai task, đối tượng IPC và ngắt

Code phần RTOS: `app_config.h`, `app_rtos.h`, `app_rtos.c` (khởi tạo, ControlTask, SensorTask, SonarTask, callback ngắt, hook lỗi) và `app_ui.c` (UiTask). ModeTask ở [`04`](04_may_trang_thai.md), CommTask ở [`05`](05_giao_thuc_bluetooth.md).

[← Về README](README.md)

---

## Thứ tự làm

| Bước | Việc | Kiểm tra |
|---|---|---|
| 1 | `app_config.h`, `app_rtos.h` | Build không lỗi |
| 2 | `app_rtos_init()` tạo đối tượng IPC + 1 task nháy LED | LED nháy, không dừng ở `configASSERT` |
| 3 | SensorTask + driver Mảng 1 | Debugger: `line.bits`, `rpmL` đổi khi di line / quay bánh |
| 4 | Callback EXTI + SonarTask | Đưa tay trước HC-SR04, `distance_cm` đổi |
| 5 | ModeTask, CommTask | Gửi `M1`, nhận `OK M1` |
| 6 | ControlTask + code Mảng 2 (đã sửa theo [`02`](02_giao_dien_module.md) mục 4), **kê xe lên** | Bánh quay đúng chiều, line lệch phải → bánh trái nhanh hơn |
| 7 | UiTask | OLED hiện đúng chế độ |

---

## 1. `app_config.h`

```c
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
```

---

## 2. `app_rtos.h`

```c
#ifndef APP_RTOS_H
#define APP_RTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "stream_buffer.h"

#include "agv_sensor_system.h"   /* Mảng 1 */
#include "control.h"             /* Mảng 2 */
#include "tb6612.h"              /* Mảng 2 */

/* ----- Bit trong egSystem ----- */
#define EVT_OBSTACLE    (1u << 0)
#define EVT_LINE_LOST   (1u << 1)
#define EVT_ESTOP       (1u << 2)
#define MODE_IDLE       (1u << 4)
#define MODE_LINE       (1u << 5)
#define MODE_FOLLOW     (1u << 6)
#define MODE_STOP       (1u << 7)
#define MODE_MASK       (MODE_IDLE | MODE_LINE | MODE_FOLLOW | MODE_STOP)

typedef enum { CMD_MODE_IDLE = 0, CMD_MODE_LINE, CMD_MODE_FOLLOW, CMD_ESTOP } ModeCmd_t;

/* ----- Kiểu dữ liệu (giải thích ở tài liệu 02, mục 2) ----- */
typedef struct { AGV_LineSample line; float rpmL, rpmR; uint32_t tick; } SensorData_t;
typedef struct { AGV_SonarSample sonar; AGV_FC51Sample fc; uint8_t servoDeg; uint32_t tick; } RangeData_t;
typedef struct { int16_t pwmL, pwmR; int8_t errSent; uint32_t tick; } CtrlOut_t;
typedef struct { float kpLine, kdLine, kpSpd, kiSpd, kpDist, vBase; uint32_t version; } Params_t;

/* ----- Đối tượng dùng chung ----- */
extern QueueHandle_t        qSensor, qRange, qCtrl, qMode;
extern SemaphoreHandle_t    semEcho, mtxParams;
extern EventGroupHandle_t   egSystem;
extern StreamBufferHandle_t sbUartRx;
extern Params_t             gParams;   /* chỉ truy cập khi giữ mtxParams */

void ModeTask(void *arg);   /* app_mode.c */
void CommTask(void *arg);   /* app_comm.c */
void UiTask(void *arg);     /* app_ui.c   */

/* ----- Chân đo thời gian PB3, PB4 (ghi thanh ghi trực tiếp, khoảng 1 chu kỳ máy) ----- */
#define DBG1_ON()   (GPIOB->BSRR = GPIO_PIN_3)
#define DBG1_OFF()  (GPIOB->BRR  = GPIO_PIN_3)
#define DBG2_ON()   (GPIOB->BSRR = GPIO_PIN_4)
#define DBG2_OFF()  (GPIOB->BRR  = GPIO_PIN_4)

void app_rtos_init(void);

#endif
```

---

## 3. `app_rtos.c`: khởi tạo

```c
#include <string.h>
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "app_rtos.h"
#include "app_config.h"

QueueHandle_t        qSensor, qRange, qCtrl, qMode;
SemaphoreHandle_t    semEcho, mtxParams;
EventGroupHandle_t   egSystem;
StreamBufferHandle_t sbUartRx;
Params_t             gParams;

static uint8_t uartRxByte;
static volatile bool echoRose;   /* ISR đặt khi Echo lên, SonarTask xóa trước mỗi lần đo */

static void ControlTask(void *arg);
static void SensorTask (void *arg);
static void SonarTask  (void *arg);

/* Lời gọi xTaskCreate nằm ngoài configASSERT: nếu configASSERT bị định nghĩa rỗng
 * thì task vẫn được tạo. */
static void create_task(TaskFunction_t fn, const char *name,
                        uint16_t stack, UBaseType_t prio)
{
    BaseType_t ok = xTaskCreate(fn, name, stack, NULL, prio, NULL);
    configASSERT(ok == pdPASS);
    (void)ok;
}

void app_rtos_init(void)
{
    /* ---- 1. Đối tượng IPC ---- */
    qSensor   = xQueueCreate(1, sizeof(SensorData_t));
    qRange    = xQueueCreate(1, sizeof(RangeData_t));
    qCtrl     = xQueueCreate(1, sizeof(CtrlOut_t));
    qMode     = xQueueCreate(4, sizeof(ModeCmd_t));
    semEcho   = xSemaphoreCreateBinary();
    mtxParams = xSemaphoreCreateMutex();
    egSystem  = xEventGroupCreate();
    sbUartRx  = xStreamBufferCreate(64, 1);
    configASSERT(qSensor && qRange && qCtrl && qMode);
    configASSERT(semEcho && mtxParams && egSystem && sbUartRx);

    vQueueAddToRegistry(qSensor, "qSensor");
    vQueueAddToRegistry(qRange,  "qRange");
    vQueueAddToRegistry(qMode,   "qMode");

    /* ---- 2. Giá trị ban đầu ---- */
    gParams = (Params_t){ DEF_KP_LINE, DEF_KD_LINE, DEF_KP_SPD,
                          DEF_KI_SPD, DEF_KP_DIST, DEF_VBASE, 0 };
    SensorData_t s0 = {0}; xQueueOverwrite(qSensor, &s0);   /* để Peek luôn có dữ liệu */
    RangeData_t  r0 = {0}; xQueueOverwrite(qRange,  &r0);
    CtrlOut_t    c0 = {0}; xQueueOverwrite(qCtrl,   &c0);
    xEventGroupSetBits(egSystem, MODE_IDLE);

    /* ---- 3. Ngoại vi: gọi hàm khởi tạo của Mảng 1 và Mảng 2 ---- */
    AGV_Sensors_Init(&htim2, &htim1, ENC_CPWR);   /* TIM2 đếm µs, servo 90°, encoder */
    control_init();                               /* 4 bộ PID                         */
    tb6612_init();                                /* TIM3 PWM, STBY = 1, PWM = 0      */
    HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);

    /* ---- 4. Task ---- */
    create_task(ControlTask, "Control", STACK_CONTROL, PRIO_CONTROL);
    create_task(SensorTask,  "Sensor",  STACK_SENSOR,  PRIO_SENSOR);
    create_task(SonarTask,   "Sonar",   STACK_SONAR,   PRIO_SONAR);
    create_task(ModeTask,    "Mode",    STACK_MODE,    PRIO_MODE);
    create_task(CommTask,    "Comm",    STACK_COMM,    PRIO_COMM);
    create_task(UiTask,      "Ui",      STACK_UI,      PRIO_UI);
}
```

Ngắt EXTI đã được bật từ `MX_GPIO_Init()` (trước khi tạo đối tượng). Callback có kiểm tra `semEcho != NULL` (mục 7) để an toàn trong khoảnh khắc đó. Sau lời gọi API FreeRTOS đầu tiên, các ngắt có ưu tiên NVIC ≥ 5 bị chặn tới khi scheduler chạy.

---

## 4. ControlTask (P5, 10 ms)

```c
#define LINE_LOST_CODE 99

static int8_t line_to_err(const AGV_LineSample *l)
{
    if (!l->valid) return LINE_LOST_CODE;
    float e = l->error;
    return (int8_t)(e >= 0.0f ? e + 0.5f : e - 0.5f);
}

static void ControlTask(void *arg)
{
    (void)arg;
    const float  dt = CONTROL_PERIOD_MS / 1000.0f;
    TickType_t   last = xTaskGetTickCount();
    EventBits_t  prevMode = 0;
    uint32_t     appliedVersion = 0;
    SensorData_t s;
    RangeData_t  r;
    Params_t     p;
    CtrlOut_t    out;

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
        DBG1_ON();

        /* 1. Copy tham số, giữ mutex ngắn nhất có thể */
        xSemaphoreTake(mtxParams, portMAX_DELAY);
        p = gParams;
        xSemaphoreGive(mtxParams);
#if CONTROL_HAS_SET_GAINS
        if (p.version != appliedVersion) {           /* có lệnh chỉnh từ điện thoại */
            control_set_gains(p.kpLine, p.kdLine, p.kpSpd, p.kiSpd, p.kpDist, p.vBase);
            appliedVersion = p.version;
        }
#else
        (void)appliedVersion;
#endif

        /* 2. Dữ liệu mới nhất */
        xQueuePeek(qSensor, &s, 0);
        xQueuePeek(qRange,  &r, 0);
        EventBits_t ev   = xEventGroupGetBits(egSystem);
        EventBits_t mode = ev & MODE_MASK;

        /* 3. Đổi chế độ -> xóa trạng thái PID cũ */
        if (mode != prevMode) { control_reset_all(); prevMode = mode; }

        /* 4. Quyết định an toàn TRƯỚC khi tính PID */
        TickType_t now = xTaskGetTickCount();
        bool stale = (now - s.tick) > pdMS_TO_TICKS(SENSOR_STALE_MS)
                  || (now - r.tick) > pdMS_TO_TICKS(RANGE_STALE_MS);
        bool stop  = (ev & (EVT_OBSTACLE | EVT_ESTOP))
                  || !(mode & (MODE_LINE | MODE_FOLLOW))
                  || stale;

        int16_t pwmL = 0, pwmR = 0;
        memset(&out, 0, sizeof out);

        if (stop) {
            control_reset_all();
        } else if (mode & MODE_LINE) {
            out.errSent = line_to_err(&s.line);
            control_line_step(out.errSent, s.rpmL, s.rpmR, dt, &pwmL, &pwmR);
        } else if (r.sonar.valid) {                  /* MODE_FOLLOW, có mục tiêu */
            control_follow_step(r.sonar.distance_cm,
                                r.fc.left_detected, r.fc.right_detected,
                                s.rpmL, s.rpmR, dt, &pwmL, &pwmR);
        } else {                                     /* FOLLOW nhưng mất mục tiêu */
            control_reset_all();
        }

        /* 5. Nơi DUY NHẤT ghi xuống động cơ */
        if (pwmL == 0 && pwmR == 0) tb6612_stop();
        else                        tb6612_set_motor(pwmL, pwmR);

        out.pwmL = pwmL;
        out.pwmR = pwmR;
        out.tick = xTaskGetTickCount();
        xQueueOverwrite(qCtrl, &out);
        DBG1_OFF();
    }
}
```

Giải thích khi bảo vệ:
- **`vTaskDelayUntil`** giữ chu kỳ đúng 10 ms, `dt` của PID luôn đúng.
- **Mặc định là dừng:** chỉ chạy khi có đúng bit chế độ và không có cờ nguy hiểm.
- **Kiểm tra dữ liệu cũ:** SensorTask hoặc SonarTask treo thì dữ liệu trong mailbox cũ đi, ControlTask tự dừng xe. SonarTask vẫn ghi `qRange` mỗi 60 ms kể cả khi không có Echo, nên siêu âm mất tín hiệu (KB7) không bị tính là dữ liệu cũ.
- **Chỉ ControlTask chạm vào các bộ PID** trong `control.c` (kể cả đổi hệ số), nên `control.c` không cần khóa.

---

## 5. SensorTask (P4, 10 ms)

```c
static void SensorTask(void *arg)
{
    (void)arg;
    TickType_t   last = xTaskGetTickCount();
    SensorData_t s = {0};
    bool         prevLost = false;
    uint32_t     wL[ENC_WIN] = {0}, wR[ENC_WIN] = {0}, wMs[ENC_WIN] = {0};
    uint8_t      k = 0;

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SENSOR_PERIOD_MS));

        s.line = AGV_Line_Read();

        /* Task DUY NHẤT gọi AGV_Encoder_Sample (hàm có trạng thái bên trong) */
        AGV_EncoderSample e = AGV_Encoder_Sample();
        wL[k] = e.left_delta;  wR[k] = e.right_delta;  wMs[k] = e.dt_ms;
        k = (uint8_t)((k + 1) % ENC_WIN);

        uint32_t sumL = 0, sumR = 0, sumMs = 0;
        for (uint8_t i = 0; i < ENC_WIN; i++) { sumL += wL[i]; sumR += wR[i]; sumMs += wMs[i]; }
        s.rpmL = sumMs ? (float)sumL * 60000.0f / (ENC_CPWR * (float)sumMs) : 0.0f;
        s.rpmR = sumMs ? (float)sumR * 60000.0f / (ENC_CPWR * (float)sumMs) : 0.0f;
        /* Khi Mảng 1 có AGV_Encoder_GetRpmPeriod(): thay 2 dòng trên bằng
           s.rpmL = AGV_Encoder_GetRpmPeriod(0); s.rpmR = AGV_Encoder_GetRpmPeriod(1); */

        s.tick = xTaskGetTickCount();
        xQueueOverwrite(qSensor, &s);

        bool lost = !s.line.valid;
        if (lost != prevLost) {                       /* chỉ báo khi đổi trạng thái */
            if (lost) xEventGroupSetBits  (egSystem, EVT_LINE_LOST);
            else      xEventGroupClearBits(egSystem, EVT_LINE_LOST);
            prevLost = lost;
        }
    }
}
```

Cửa sổ trượt 100 ms giảm nhiễu khi đếm xung (xem [`02`](02_giao_dien_module.md) mục 3.2), đổi lại rpm trễ khoảng 50 ms.

---

## 6. SonarTask (P3, 60 ms)

```c
static void SonarTask(void *arg)
{
    (void)arg;
    static const uint8_t sweep[4] = { 90, 45, 90, 135 };
    TickType_t  last = xTaskGetTickCount();
    RangeData_t r = {0};
    uint8_t     deg = SERVO_CENTER, idx = 0, hold = 0, clearCnt = 0;

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SONAR_PERIOD_MS));
        EventBits_t ev = xEventGroupGetBits(egSystem);
        bool stopMode  = (ev & MODE_STOP) != 0;

        /* --- Servo: quét khi STOP, giữ mỗi góc 300 ms cho servo ổn định --- */
        if (stopMode) {
            if (++hold >= SERVO_HOLD_CYCLES) {
                hold = 0;
                idx  = (uint8_t)((idx + 1) % 4);
                deg  = sweep[idx];
                AGV_Servo_SetAngle(&htim1, deg);
            }
        } else if (deg != SERVO_CENTER) {
            deg = SERVO_CENTER; idx = 0; hold = 0;
            AGV_Servo_SetAngle(&htim1, deg);
        }
        bool settled = !stopMode || hold >= SERVO_SETTLE_CYCLES;

        /* --- Đo: Poll dọn trạng thái cũ -> Trigger -> Blocked chờ Echo --- */
        (void)AGV_Sonar_Poll();            /* lần đo trước còn treo -> đánh dấu TIMEOUT */
        xSemaphoreTake(semEcho, 0);        /* xóa semaphore còn sót */
        echoRose = false;
        bool triggered = AGV_Sonar_Trigger();   /* false: Echo lần trước còn ở mức cao */
        if (triggered) {
            xSemaphoreTake(semEcho, pdMS_TO_TICKS(AGV_SONAR_TIMEOUT_MS));
        }
        r.sonar    = AGV_Sonar_Poll();     /* READY -> valid; quá hạn -> valid = false */
        r.fc       = AGV_FC51_Read();
        r.servoDeg = deg;
        r.tick     = xTaskGetTickCount();
        xQueueOverwrite(qRange, &r);

        /* --- Vật cản: có trễ (hysteresis), chỉ khi đã phát đo, nhìn thẳng và servo đứng yên --- */
        if (triggered && deg == SERVO_CENTER && settled) {
            float thr = (ev & MODE_FOLLOW) ? OBSTACLE_CM_FOLLOW : OBSTACLE_CM_LINE;
            /* Echo có lên nhưng không xuống kịp: không có vật trong tầm đo */
            bool noTarget = echoRose && (r.sonar.state != AGV_SONAR_READY);
            if (r.sonar.valid && r.sonar.distance_cm < thr) {
                xEventGroupSetBits(egSystem, EVT_OBSTACLE);
                clearCnt = 0;
            } else if (noTarget || (r.sonar.valid && r.sonar.distance_cm > OBSTACLE_CLEAR_CM)) {
                if (++clearCnt >= OBSTACLE_CLEAR_COUNT) {
                    xEventGroupClearBits(egSystem, EVT_OBSTACLE);
                    clearCnt = OBSTACLE_CLEAR_COUNT;
                }
            } else {
                clearCnt = 0;              /* giữa ngưỡng và 20 cm, hoặc cảm biến không
                                              phản hồi: giữ nguyên trạng thái */
            }
        }
    }
}
```

- **Không sửa driver Mảng 1:** `AGV_Sonar_Trigger()` từ chối phát nếu lần đo trước còn treo. Gọi `AGV_Sonar_Poll()` trước để driver tự chuyển lần đo cũ sang TIMEOUT.
- **Chờ bằng semaphore:** trong 0–35 ms chờ Echo, SonarTask Blocked, không tốn CPU. Có timeout nên rút dây Echo cũng không kẹt.
- **Hysteresis:** vào vật cản khi < 15 cm, chỉ thoát khi > 20 cm trong 3 lần đo liên tiếp, tránh bật/tắt liên tục quanh ngưỡng.
- **Chỉ đánh giá khi servo nhìn thẳng và đã đứng yên** (theo lưu ý của Mảng 1: servo cần 150–300 ms để ổn định).
- **Không đo được thì không coi là đường thông.** Echo có lên nhưng không xuống trong 35 ms nghĩa là phía trước trống, được tính là một lần thông. Echo không lên (`echoRose == false`) nghĩa là cảm biến không phản hồi, `EVT_OBSTACLE` giữ nguyên nên xe đang STOP không tự chạy lại. Xung quá ngắn (< 100 µs, vật sát cảm biến) có `state == READY` nên cũng không được tính là thông.
- **Chu kỳ không phát đo được thì bỏ qua đánh giá.** Một số module HC-SR04 giá rẻ giữ Echo ở mức cao tới khoảng 200 ms khi không có vọng về, làm `AGV_Sonar_Trigger()` từ chối phát ở vài chu kỳ sau. Nếu các chu kỳ đó xóa `clearCnt` thì bộ đếm không bao giờ tới 3 và xe kẹt ở STOP. Bỏ qua chúng thì bộ đếm giữ nguyên; Echo kẹt mức cao thật thì không có lần đo nào được tính, xe vẫn đứng yên.

---

## 7. Callback ngắt

Đặt trong `app_rtos.c`. **Toàn firmware chỉ có một `HAL_GPIO_EXTI_Callback`.**

```c
/* EXTI6, EXTI7 (encoder), EXTI8 (Echo): chung ngắt EXTI9_5, ưu tiên NVIC 5 */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    AGV_Sensors_OnEXTI(pin);                      /* Mảng 1: đếm xung, đo Echo */

    if (pin == AGV_ECHO_PIN &&
        HAL_GPIO_ReadPin(AGV_ECHO_PORT, AGV_ECHO_PIN) == GPIO_PIN_SET) {
        echoRose = true;                          /* cảm biến có phản hồi */
    }

    if (pin == AGV_ECHO_PIN && semEcho != NULL &&
        HAL_GPIO_ReadPin(AGV_ECHO_PORT, AGV_ECHO_PIN) == GPIO_PIN_RESET) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(semEcho, &woken);   /* đánh thức SonarTask */
        portYIELD_FROM_ISR(woken);
    }
}

/* USART1: nhận từng byte từ HC-05 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    BaseType_t woken = pdFALSE;
    xStreamBufferSendFromISR(sbUartRx, &uartRxByte, 1, &woken);
    HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
    portYIELD_FROM_ISR(woken);
}

/* Lỗi overrun làm UART ngừng nhận: xóa cờ, bật lại */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    __HAL_UART_CLEAR_OREFLAG(huart);
    HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
}
```

`HAL_UART_TxCpltCallback` nằm trong `app_comm.c` ([`05`](05_giao_thuc_bluetooth.md)).

---

## 8. Hook lỗi

Viết vào vùng `USER CODE` của các hàm CubeMX sinh sẵn trong `freertos.c`:

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; (void)pcTaskName;   /* breakpoint ở đây, xem pcTaskName */
    tb6612_stop();                   /* an toàn trước tiên */
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    tb6612_stop();
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
```

---

## 9. UiTask (P1): `app_ui.c`

Dùng thư viện `stm32-ssd1306` (afiskon, giấy phép MIT), đã có sẵn trong `Core/Inc`, `Core/Src`. `ssd1306_conf.h` đã đặt STM32F1, I2C2 (`hi2c2`) và chỉ bật font 7x10, vì các font còn lại tốn nhiều Flash. `ssd1306_Init()` có `HAL_Delay(100)` nhưng chạy ở UiTask (P1) nên không cản task khác.

```c
#include <stdio.h>
#include "main.h"
#include "app_rtos.h"
#include "app_config.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#define LED_G(x) HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LED_R(x) HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define BUZ(x)   HAL_GPIO_WritePin(BUZZER_GPIO_Port,    BUZZER_Pin,    (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

static const char *mode_name(EventBits_t ev)
{
    if (ev & MODE_LINE)   return "LINE";
    if (ev & MODE_FOLLOW) return "FOLLOW";
    if (ev & MODE_STOP)   return "STOP";
    return "IDLE";
}

void UiTask(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t   n = 0;
    SensorData_t s; RangeData_t r; CtrlOut_t c;
    char line[22];

    ssd1306_Init();

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(UI_PERIOD_MS));
        n++;
        HAL_GPIO_TogglePin(LED_BOARD_GPIO_Port, LED_BOARD_Pin);   /* "còn sống" */
        EventBits_t ev = xEventGroupGetBits(egSystem);

        /* --- LED, còi: mỗi 100 ms --- */
        if (ev & (MODE_STOP | EVT_ESTOP))                 { LED_G(0); LED_R(1); BUZ((n % 4) < 2); }
        else if ((ev & MODE_IDLE) && (ev & EVT_LINE_LOST)) { LED_G(0); LED_R(n & 1); BUZ(0); }
        else if (ev & MODE_LINE)                          { LED_G(1); LED_R(0); BUZ(0); }
        else if (ev & MODE_FOLLOW)                        { LED_G(n & 1); LED_R(0); BUZ(0); }
        else                                              { LED_G((n % 10) < 5); LED_R(0); BUZ(0); }

        /* --- OLED: mỗi 200 ms --- */
        if (n & 1) continue;
        xQueuePeek(qSensor, &s, 0);
        xQueuePeek(qRange,  &r, 0);
        xQueuePeek(qCtrl,   &c, 0);

        ssd1306_Fill(Black);
        snprintf(line, sizeof line, "Mode: %s", mode_name(ev));
        ssd1306_SetCursor(0, 0);  ssd1306_WriteString(line, Font_7x10, White);

        /* bit 4 = mắt trái: in từ trái sang phải */
        snprintf(line, sizeof line, "Line %c%c%c%c%c e%+d",
                 (s.line.bits & 0x10) ? '#' : '.', (s.line.bits & 0x08) ? '#' : '.',
                 (s.line.bits & 0x04) ? '#' : '.', (s.line.bits & 0x02) ? '#' : '.',
                 (s.line.bits & 0x01) ? '#' : '.', (int)(s.line.error * 10));
        ssd1306_SetCursor(0, 12); ssd1306_WriteString(line, Font_7x10, White);

        snprintf(line, sizeof line, "rpm %3d/%3d", (int)s.rpmL, (int)s.rpmR);
        ssd1306_SetCursor(0, 24); ssd1306_WriteString(line, Font_7x10, White);

        snprintf(line, sizeof line, "PWM %4d/%4d", c.pwmL, c.pwmR);
        ssd1306_SetCursor(0, 36); ssd1306_WriteString(line, Font_7x10, White);

        if (r.sonar.valid) snprintf(line, sizeof line, "Dist %3d cm", (int)r.sonar.distance_cm);
        else               snprintf(line, sizeof line, "Dist  --");
        ssd1306_SetCursor(0, 48); ssd1306_WriteString(line, Font_7x10, White);

        ssd1306_UpdateScreen();   /* I2C blocking ~20 ms, task khác chen ngang thoải mái */
    }
}
```

`e%+d` in sai số nhân 10 (ví dụ `e+15` = 1,5). Không dùng `%f`: in số thực với newlib-nano phải bật `-u _printf_float`, tốn khoảng 10 KB Flash và nhiều stack.

---

## 10. Tóm tắt API FreeRTOS đã dùng

| API | Dùng ở | Tác dụng |
|---|---|---|
| `xTaskCreate` | `app_rtos_init` | Tạo task với stack và ưu tiên |
| `vTaskDelayUntil` | Task định kỳ | Chu kỳ chính xác, không trôi |
| `xQueueCreate(1, …)` + `xQueueOverwrite` / `xQueuePeek` | Mailbox cảm biến | Luôn giữ giá trị mới nhất |
| `xQueueSend` / `xQueueReceive` | `qMode` | Hàng đợi lệnh FIFO |
| `xSemaphoreCreateBinary` + `GiveFromISR` / `Take` | `semEcho` | Ngắt báo task, có timeout |
| `xSemaphoreCreateMutex` + `Take` / `Give` | `mtxParams` | Khóa tài nguyên, kế thừa ưu tiên |
| `xEventGroupSetBits` / `ClearBits` / `GetBits` | `egSystem` | Cờ sự kiện, chế độ |
| `xStreamBufferSendFromISR` / `xStreamBufferReceive` | `sbUartRx` | Luồng byte từ ngắt UART |
| `portYIELD_FROM_ISR` | Mọi ISR | Chuyển ngữ cảnh ngay khi thoát ngắt |
| `configASSERT` | Khởi tạo | Dừng ngay khi tạo đối tượng thất bại |
