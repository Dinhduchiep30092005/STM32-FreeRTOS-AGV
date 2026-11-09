#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "usart.h"
#include "app_rtos.h"
#include "app_config.h"

static SemaphoreHandle_t semTxDone;
static char txBuf[128];
static bool telemetryOn = true;

/* ===================== Gửi ===================== */
static void comm_send(const char *s)
{
    size_t n = strlen(s);
    if (n > sizeof txBuf) n = sizeof txBuf;
    memcpy(txBuf, s, n);

    /* USART1 (NVIC 6) bị che trong critical section, nên ngắt RX không thể chen vào
     * lúc HAL đang giữ __HAL_LOCK(huart); nếu không, HAL_UART_Receive_IT trong ngắt
     * trả HAL_BUSY và UART ngừng nhận vĩnh viễn. */
    taskENTER_CRITICAL();
    HAL_StatusTypeDef st = HAL_UART_Transmit_IT(&huart1, (uint8_t *)txBuf, (uint16_t)n);
    taskEXIT_CRITICAL();

    if (st == HAL_OK) {
        /* Blocked tới khi phần cứng gửi xong, không tốn CPU */
        xSemaphoreTake(semTxDone, pdMS_TO_TICKS(200));
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(semTxDone, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ===================== Tham số ===================== */
typedef struct { const char *name; float *ptr; float lo, hi; } ParamDef_t;

static bool set_param(const char *name, float v)
{
    /* Bảng ánh xạ tên lệnh -> trường trong gParams */
    ParamDef_t tbl[] = {
        { "KP1", &gParams.kpLine, 0, 200 },
        { "KD1", &gParams.kdLine, 0,  50 },
        { "KP2", &gParams.kpSpd,  0,  50 },
        { "KI2", &gParams.kiSpd,  0,  50 },
        { "KPD", &gParams.kpDist, 0,  50 },
        { "VB",  &gParams.vBase,  0, 250 },
    };
    for (size_t i = 0; i < sizeof tbl / sizeof tbl[0]; i++) {
        if (strcmp(name, tbl[i].name) == 0) {
            /* Viết dạng !(trong khoảng) để NaN cũng bị từ chối */
            if (!(v >= tbl[i].lo && v <= tbl[i].hi)) return false;
            xSemaphoreTake(mtxParams, portMAX_DELAY);   /* vùng giữ mutex: vài µs */
            *tbl[i].ptr = v;
            gParams.version++;                          /* báo ControlTask áp dụng */
            xSemaphoreGive(mtxParams);
            return true;
        }
    }
    return false;
}

/* ===================== Xử lý một dòng lệnh ===================== */
static void send_mode_cmd(ModeCmd_t c, const char *okMsg)
{
    if (xQueueSend(qMode, &c, pdMS_TO_TICKS(10)) == pdTRUE) comm_send(okMsg);
    else                                                    comm_send("ERR BUSY\r\n");
}

static void handle_line(char *line)
{
    char out[96];

    if      (strcmp(line, "M0") == 0) send_mode_cmd(CMD_MODE_IDLE,   "OK M0\r\n");
    else if (strcmp(line, "M1") == 0) send_mode_cmd(CMD_MODE_LINE,   "OK M1\r\n");
    else if (strcmp(line, "M2") == 0) send_mode_cmd(CMD_MODE_FOLLOW, "OK M2\r\n");
    else if (strcmp(line, "S")  == 0) send_mode_cmd(CMD_ESTOP,       "OK STOP\r\n");
    else if (strcmp(line, "T1") == 0) { telemetryOn = true;  comm_send("OK T1\r\n"); }
    else if (strcmp(line, "T0") == 0) { telemetryOn = false; comm_send("OK T0\r\n"); }
    else if (strcmp(line, "GET") == 0) {
        Params_t p;
        xSemaphoreTake(mtxParams, portMAX_DELAY); p = gParams; xSemaphoreGive(mtxParams);
        snprintf(out, sizeof out,
                 "P KP1=%d KD1=%d KP2=%d KI2=%d KPD=%d VB=%d (x100)\r\n",
                 (int)(p.kpLine * 100), (int)(p.kdLine * 100), (int)(p.kpSpd * 100),
                 (int)(p.kiSpd * 100),  (int)(p.kpDist * 100), (int)(p.vBase * 100));
        comm_send(out);
    }
    else if (strcmp(line, "STK") == 0) {
        static TaskStatus_t st[8];   /* static: không chiếm stack của CommTask */
        UBaseType_t n = uxTaskGetSystemState(st, 8, NULL);
        for (UBaseType_t i = 0; i < n; i++) {
            snprintf(out, sizeof out, "STK %s %u\r\n",
                     st[i].pcTaskName, (unsigned)st[i].usStackHighWaterMark);
            comm_send(out);
        }
    }
#if (configGENERATE_RUN_TIME_STATS == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS > 0)
    else if (strcmp(line, "CPU") == 0) {
        static char buf[400];            /* static: không chiếm stack */
        vTaskGetRunTimeStats(buf);
        /* Kết quả dài hơn txBuf (128 byte) nên phải gửi từng đoạn */
        size_t left = strlen(buf);
        for (const char *p = buf; left > 0; ) {
            comm_send(p);
            size_t sent = (left > sizeof txBuf) ? sizeof txBuf : left;
            p += sent;
            left -= sent;
        }
    }
#endif
    else {
        /* Dạng "TEN GIA_TRI" */
        char *space = strchr(line, ' ');
        if (space) {
            *space = '\0';
            char *end;
            float v = strtof(space + 1, &end);
            /* end == space + 1: không đọc được số, không được hiểu thành 0 */
            if (end != space + 1 && set_param(line, v)) {
                snprintf(out, sizeof out, "OK %s=%d.%02d\r\n",
                         line, (int)v, abs((int)(v * 100)) % 100);
                comm_send(out);
            } else {
                comm_send("ERR RANGE\r\n");
            }
        } else {
            comm_send("ERR CMD\r\n");
        }
    }
}

/* ===================== Telemetry ===================== */
static void send_telemetry(void)
{
    SensorData_t s; RangeData_t r; CtrlOut_t c;
    xQueuePeek(qSensor, &s, 0);
    xQueuePeek(qRange,  &r, 0);
    xQueuePeek(qCtrl,   &c, 0);
    EventBits_t ev = xEventGroupGetBits(egSystem);
    int mode = (ev & MODE_LINE) ? 1 : (ev & MODE_FOLLOW) ? 2 : (ev & MODE_STOP) ? 3 : 0;

    int err10 = s.line.valid ? (int)(s.line.error * 10.0f) : 99;
    char out[80];
    snprintf(out, sizeof out, "T,%lu,%d,%u,%d,%d,%d,%d,%d,%d\r\n",
             (unsigned long)xTaskGetTickCount(), mode, s.line.bits, err10,
             (int)s.rpmL, (int)s.rpmR, c.pwmL, c.pwmR,
             r.sonar.valid ? (int)r.sonar.distance_cm : -1);
    comm_send(out);
}

/* ===================== Task ===================== */
void CommTask(void *arg)
{
    (void)arg;
    semTxDone = xSemaphoreCreateBinary();
    configASSERT(semTxDone);

    char       line[32];
    uint8_t    len = 0;
    TickType_t lastTel = xTaskGetTickCount();

    comm_send("AGV READY\r\n");

    for (;;) {
        uint8_t ch;
        /* Chờ byte tối đa 10 ms, để còn kịp gửi telemetry đúng hẹn */
        if (xStreamBufferReceive(sbUartRx, &ch, 1, pdMS_TO_TICKS(10)) == 1) {
            if (ch == '\r') {
                /* bỏ qua */
            } else if (ch == '\n') {
                line[len] = '\0';
                if (len > 0) handle_line(line);
                len = 0;
            } else if (len < sizeof line - 1) {
                line[len++] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : (char)ch;
            } else {
                len = 0;   /* dòng quá dài: bỏ */
            }
        }

        if (telemetryOn &&
            (xTaskGetTickCount() - lastTel) >= pdMS_TO_TICKS(COMM_TELEMETRY_MS)) {
            lastTel += pdMS_TO_TICKS(COMM_TELEMETRY_MS);
            send_telemetry();
        }
    }
}
