#include <string.h>
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "app_rtos.h"
#include "app_config.h"

#define LINE_LOST_CODE 99

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

/* ===================== ControlTask (P5, 10 ms) ===================== */
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

/* ===================== SensorTask (P4, 10 ms) ===================== */
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

/* ===================== SonarTask (P3, 60 ms) ===================== */
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

/* ===================== Callback ngắt ===================== */

/* EXTI6, EXTI7 (encoder), EXTI8 (Echo): chung ngắt EXTI9_5, ưu tiên NVIC 5.
 * Toàn firmware chỉ có một HAL_GPIO_EXTI_Callback. */
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

/* ===================== Đo tải CPU (chỉ khi bật run-time stats) ===================== */
#if (configGENERATE_RUN_TIME_STATS == 1)
void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* bật khối trace   */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;             /* bật đếm chu kỳ   */
}

uint32_t dwt_get_us(void)
{
    return DWT->CYCCNT / 72;                          /* 72 MHz -> µs     */
}
#endif
