# 05. Giao tiếp Bluetooth (HC-05) và CommTask

Điện thoại giao tiếp với xe qua module HC-05 (Bluetooth Classic, cổng COM ảo). Tài liệu gồm: đấu nối, cấu hình HC-05, danh sách lệnh, định dạng telemetry và code CommTask.

[← Về README](README.md)

---

## 1. Đấu nối

| HC-05 | Blue Pill | Ghi chú |
|---|---|---|
| VCC | 5 V (từ LM2596) | Module có đế ổn áp, chịu 3,6–6 V |
| GND | GND | Nối chung |
| TXD | PA10 (USART1 RX) | Mức 3,3 V, nối thẳng |
| RXD | PA9 (USART1 TX) | Mức 3,3 V, nối thẳng |
| EN / KEY | Để trống | Chỉ dùng khi vào chế độ lệnh AT |
| STATE | Để trống | Lên mức cao khi đã kết nối (có thể dùng sau) |

TX của bên này nối RX của bên kia. Nối nhầm TX–TX là lỗi phổ biến nhất khi không nhận được dữ liệu.

**Lưu ý:** HC-05 là Bluetooth Classic, **không kết nối được với iPhone**. Cần ít nhất một điện thoại Android hoặc laptop có Bluetooth.

---

## 2. Cấu hình HC-05 (không bắt buộc)

Mặc định: tốc độ **9600 baud**, tên `HC-05`, mã ghép nối `1234` hoặc `0000`. Dùng luôn mặc định thì bỏ qua mục này và để USART1 ở 9600.

Nên nâng lên 115200 baud: ở 9600 baud, gửi một dòng telemetry 50 byte mất khoảng 52 ms; ở 115200 chỉ còn khoảng 4 ms.

**Vào chế độ lệnh AT:**
1. Ngắt nguồn HC-05.
2. Giữ nút nhỏ trên module (hoặc nối chân EN lên 3,3 V), rồi cấp nguồn. LED trên module nháy **chậm** (khoảng 2 giây một lần) là đã vào chế độ AT.
3. Chế độ AT dùng **38400 baud**. Kết nối HC-05 với máy tính qua mạch USB–UART, hoặc tạm dùng Blue Pill làm cầu nối (chuyển byte giữa USART1 và USART2).
4. Gửi lần lượt (mỗi lệnh kết thúc bằng `\r\n`):

```
AT                       → OK
AT+NAME=AGV-NHOM1        → OK      (đặt tên để dễ tìm, tránh nhầm với nhóm khác)
AT+PSWD=1234             → OK      (một số bản firmware cần dấu ngoặc: AT+PSWD="1234")
AT+UART=115200,0,0       → OK
```

5. Ngắt nguồn, cấp lại bình thường (không giữ nút). Đổi USART1 trong CubeMX thành 115200 baud.

---

## 3. App trên điện thoại

| App (Android) | Dùng để |
|---|---|
| **Serial Bluetooth Terminal** (Kai Morich) | Gõ lệnh dạng chữ, xem phản hồi và telemetry. Tạo được nút macro cho `M1`, `M2`, `S` |
| **Bluetooth Electronics** (keuwlsoft) | Kéo thả nút bấm, thanh trượt, đồ thị thành bảng điều khiển. Hợp để demo |

Trong Serial Bluetooth Terminal: **Settings → Send → Newline = LF** (lệnh phải kết thúc bằng `\n`).

Vẽ đồ thị trên laptop: ghép nối HC-05 với laptop, Windows tạo một cổng COM ảo. Mở bằng Serial Plotter, hoặc script Python dùng `pyserial` để đọc dòng `T,...` và vẽ bằng `matplotlib`.

---

## 4. Danh sách lệnh

Mỗi lệnh là một dòng chữ in hoa, kết thúc bằng `\n`. Xe luôn trả lời `OK ...` hoặc `ERR ...`.

### 4.1. Lệnh chế độ

| Lệnh | Tác dụng | Phản hồi |
|---|---|---|
| `M0` | Về IDLE | `OK M0` |
| `M1` | Chế độ bám line | `OK M1` |
| `M2` | Chế độ bám vật thể | `OK M2` |
| `S` | Dừng khẩn (về IDLE, đặt `EVT_ESTOP`) | `OK STOP` |

### 4.2. Lệnh tham số

| Lệnh | Tham số (trường trong `gParams`) | Khoảng hợp lệ | Mặc định (`control_init`) |
|---|---|---|---|
| `KP1 x` | Kp vòng ngoài bám line (`kpLine`) | 0 … 200 | 45 |
| `KD1 x` | Kd vòng ngoài (`kdLine`) | 0 … 50 | 3,5 |
| `KP2 x` | Kp vòng trong tốc độ bánh (`kpSpd`) | 0 … 50 | 7,5 |
| `KI2 x` | Ki vòng trong (`kiSpd`) | 0 … 50 | 1,2 |
| `KPD x` | Kp khoảng cách (`kpDist`) | 0 … 50 | 8 |
| `VB x` | Tốc độ cơ bản, rpm (`vBase`) | 0 … 250 | 120 |

Phản hồi: `OK KP1=50.00`. Giá trị ngoài khoảng: `ERR RANGE`.

Lệnh tham số chỉ có tác dụng trên xe khi Mảng 2 đã thêm `control_set_gains()` và đặt `CONTROL_HAS_SET_GAINS 1` ([`02`](02_giao_dien_module.md), mục 4, việc 7). Trước đó CommTask vẫn nhận và lưu giá trị nhưng ControlTask chưa áp dụng.

### 4.3. Lệnh xem thông tin

| Lệnh | Phản hồi |
|---|---|
| `GET` | Toàn bộ tham số hiện tại trên một dòng |
| `T1` / `T0` | Bật / tắt gửi telemetry |
| `STK` | Lượng stack còn trống ít nhất của từng task (word), để chỉnh `STACK_*` |
| `CPU` | Phần trăm thời gian CPU của từng task (chỉ khi bật `GENERATE_RUN_TIME_STATS`, xem [`06`](06_kiem_thu_va_do_dac.md)) |

---

## 5. Định dạng telemetry

Khi bật (`T1`, mặc định bật), mỗi 100 ms xe gửi một dòng:

```
T,<tick>,<mode>,<bits>,<err10>,<rpmL>,<rpmR>,<pwmL>,<pwmR>,<cm>
```

| Trường | Ý nghĩa |
|---|---|
| `tick` | Thời điểm (ms từ lúc khởi động) |
| `mode` | 0 = IDLE, 1 = LINE, 2 = FOLLOW, 3 = STOP |
| `bits` | 5 mắt dò line (0…31), **bit 4 = mắt trái** |
| `err10` | `AGV_Line_Read().error × 10` (−20…+20); `99` nếu mất line |
| `rpmL`, `rpmR` | Tốc độ bánh, số nguyên |
| `pwmL`, `pwmR` | PWM thực tế đã xuất |
| `cm` | Khoảng cách, `-1` nếu không hợp lệ |

Ví dụ: `T,15230,1,4,0,118,121,520,535,-1`

Mọi số đều gửi dưới dạng số nguyên, không dùng `printf("%f")` (lý do ở [`03`](03_trien_khai_task.md), mục 9).

---

## 6. Code `app_comm.c`

Điểm thiết kế quan trọng: **gửi bằng ngắt + semaphore** thay vì `HAL_UART_Transmit` kiểu blocking. `HAL_UART_Transmit` chờ chủ động (vòng lặp kiểm tra cờ) suốt thời gian gửi. Ở 9600 baud, CommTask sẽ chiếm CPU khoảng 50 ms trong mỗi 100 ms, và UiTask (ưu tiên thấp hơn) gần như không được chạy. Dùng `HAL_UART_Transmit_IT`, CommTask chuyển sang Blocked trong lúc phần cứng gửi, CPU dành cho task khác.

```c
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
    if (HAL_UART_Transmit_IT(&huart1, (uint8_t *)txBuf, n) == HAL_OK) {
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
            if (v < tbl[i].lo || v > tbl[i].hi) return false;
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
    else {
        /* Dạng "TEN GIA_TRI" */
        char *space = strchr(line, ' ');
        if (space) {
            *space = '\0';
            float v = strtof(space + 1, NULL);
            if (set_param(line, v)) {
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
```

Ghi chú:

- `strtof` kéo thêm khoảng 3–5 KB Flash, vẫn vừa 64 KB. Nếu thiếu Flash thì tự viết hàm đổi chuỗi sang số.
- Lệnh tự đổi sang chữ in hoa, nên gõ `m1` hay `M1` đều được.
- `send_mode_cmd` gửi vào `qMode` với timeout 10 ms. Queue đầy (ModeTask bị treo) thì báo `ERR BUSY` thay vì làm CommTask kẹt theo.

---

## 7. Kiểm thử

| # | Gửi | Mong đợi | Đạt |
|---|---|---|---|
| 1 | (bật nguồn, kết nối) | Nhận `AGV READY` | ☐ |
| 2 | `GET` | Dòng `P KP1=4500 KD1=350 ...` khớp `app_config.h` | ☐ |
| 3 | `KP1 50` rồi `GET` | `OK KP1=50.00`, `GET` thấy `KP1=5000` | ☐ |
| 4 | `KP1 999` | `ERR RANGE`, giá trị không đổi | ☐ |
| 5 | `ABC` | `ERR CMD` | ☐ |
| 6 | `T1` | Các dòng `T,...` đều đặn khoảng 10 dòng/giây | ☐ |
| 7 | Gửi liên tục 20 lệnh `GET` thật nhanh | Không treo; UART vẫn nhận lệnh sau đó | ☐ |
| 8 | `STK` | Mỗi task còn ≥ 30 word trống. Nếu ít hơn thì tăng `STACK_*` | ☐ |
