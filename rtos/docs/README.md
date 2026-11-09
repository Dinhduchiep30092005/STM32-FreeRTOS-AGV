# Xe AGV bám line / bám vật thể — Phần RTOS (Mảng 3)

Tài liệu mô tả phần **hệ điều hành thời gian thực** của xe AGV: kiến trúc task, đối tượng giao tiếp giữa các task, diễn biến hệ thống theo từng kịch bản và phân tích thời gian. Phần này **ghép** code của hai mảng còn lại:

| Mảng | Thư mục | Nội dung |
|---|---|---|
| Mảng 1: cảm biến | `Core/Inc`, `Core/Src`; tài liệu ở `hardware/docs` | `agv_line`, `agv_encoder`, `agv_sonar`, `agv_servo`, `agv_fc51`, `agv_sensor_system` + `Tut.md` (sơ đồ chân toàn xe) |
| Mảng 2: điều khiển | `Core/Inc`, `Core/Src`; test trên máy tính ở `tests` | `pid`, `control`, `tb6612` |
| Mảng 3: RTOS | `Core/Inc`, `Core/Src`; tài liệu ở `rtos/docs` | Tài liệu này + `app_rtos.c`, `app_mode.c`, `app_comm.c`, `app_ui.c`, thư viện OLED `ssd1306` |

| Mục | Nội dung |
|---|---|
| Vi điều khiển | STM32F103C8T6 (Blue Pill), Cortex-M3, 72 MHz, 64 KB Flash, 20 KB RAM, không có FPU |
| RTOS | FreeRTOS (CubeIDE, interface CMSIS-RTOS v2; trong code dùng API FreeRTOS gốc) |
| Công cụ | STM32CubeIDE, ST-Link V2 |

> **Trước khi ghép code:** code PID ban đầu có **2 lỗi dấu** làm xe chạy ngược hướng và **1 lỗi chọn timer** trùng với servo, các lỗi này đã được sửa ngày 9/11. Chi tiết ở [`02_giao_dien_module.md`](02_giao_dien_module.md), mục 4.

---

## Mục lục

1. [Kiến trúc tổng quan](#1-kiến-trúc-tổng-quan)
2. [Danh sách task](#2-danh-sách-task)
3. [Đối tượng IPC và bit sự kiện](#3-đối-tượng-ipc-và-bit-sự-kiện)
4. [Các chế độ hoạt động](#4-các-chế-độ-hoạt-động)
5. [Diễn biến và kịch bản](#5-diễn-biến-và-kịch-bản)
6. [Phân tích thời gian](#6-phân-tích-thời-gian)
7. [Ghép code của các mảng](#7-ghép-code-của-các-mảng)
8. [Cấu trúc thư mục project](#8-cấu-trúc-thư-mục-project)
9. [Tài liệu chi tiết](#9-tài-liệu-chi-tiết)

---

## 1. Kiến trúc tổng quan

```
 NGẮT (ISR)                  TASK (ưu tiên)                ĐỐI TƯỢNG IPC                  ĐIỀU KHIỂN            NGOẠI VI
 ──────────────────          ──────────────────            ───────────────────            ──────────────        ─────────────
 EXTI6, EXTI7 (PB6, PB7) ─┐
 encoder: đếm xung        │  SensorTask  P4 · 10ms ──────► qSensor (mailbox)      ──┐
                          └► AGV_Line_Read()          └──► egSystem: LINE_LOST      │
                             AGV_Encoder_Sample()                                   │
                                                                                     │    ┌──────────────┐
 EXTI8 (PB8) Echo ───────►   SonarTask   P3 · 60ms ──────► qRange  (mailbox)      ──┼──► │ ControlTask  │ ──► TB6612 (TIM3)
 give semEcho                AGV_Sonar_*, servo, FC-51 └──► egSystem: OBSTACLE  ──┤    │ P5 · 10ms    │     2 động cơ
                                                                                     │    │ control_*()  │
                             ModeTask    P3 · sự kiện ───► egSystem: MODE_*     ──┤    │ tb6612_*()   │ ──► qCtrl (mailbox)
                                  ▲ qMode (queue)                                    │    └──────────────┘
 USART1 RX ───────────────►  CommTask    P2 · sự kiện ───► mtxParams (mutex)    ──┘
 stream buffer                lệnh, telemetry       ◄──── qCtrl, qSensor, qRange

                             UiTask      P1 · 100ms  ◄──── mailbox + egSystem ─────────────────────────────────► OLED (I2C2), LED, còi
```

Sơ đồ khối: [`agv_rtos_architecture.png`](agv_rtos_architecture.png).

Ba nguyên tắc thiết kế:

1. **ISR làm ít nhất có thể.** Ngắt EXTI chỉ gọi `AGV_Sensors_OnEXTI()` của Mảng 1 (đếm xung, ghi thời điểm cạnh Echo), sau đó báo cho task bằng semaphore. Mọi tính toán nằm trong task.
2. **Dữ liệu cảm biến truyền bằng mailbox** (queue dài 1, ghi bằng `xQueueOverwrite`, đọc bằng `xQueuePeek`). Bên đọc luôn lấy giá trị mới nhất.
3. **ControlTask là nơi duy nhất gọi `tb6612_set_motor()` / `tb6612_stop()`.** Task khác muốn dừng xe chỉ đặt bit trong event group.

---

## 2. Danh sách task

Trong FreeRTOS, **số ưu tiên càng lớn thì ưu tiên càng cao**.

| Task | Ưu tiên | Chu kỳ / kích hoạt | Stack (word) | Nhiệm vụ | Gọi code của mảng khác |
|---|---|---|---|---|---|
| **ControlTask** | 5 | 10 ms (`vTaskDelayUntil`) | 256 | Chọn nhánh điều khiển theo chế độ, gọi PID, ghi PWM | `control_line_step`, `control_follow_step`, `control_reset_all`, `tb6612_set_motor`, `tb6612_stop` |
| **SensorTask** | 4 | 10 ms | 160 | Đọc line, lấy mẫu encoder, làm mượt rpm | `AGV_Line_Read`, `AGV_Encoder_Sample` (**task duy nhất được gọi**) |
| **SonarTask** | 3 | 60 ms | 192 | Phát Trig, chờ `semEcho`, đọc kết quả; quét servo; đọc FC-51 | `AGV_Sonar_Poll`, `AGV_Sonar_Trigger`, `AGV_Servo_SetAngle`, `AGV_FC51_Read` |
| **ModeTask** | 3 | Chờ `qMode` (timeout 20 ms) | 128 | Máy trạng thái IDLE / LINE / FOLLOW / STOP | – |
| **CommTask** | 2 | Chờ stream buffer (timeout 10 ms) | 320 | Parse lệnh từ điện thoại; gửi telemetry 100 ms/lần | – |
| **UiTask** | 1 | 100 ms (OLED vẽ lại mỗi 200 ms) | 320 | OLED, LED, còi | – |

Lý do sắp xếp ưu tiên:

- **ControlTask cao nhất** vì quyết định trực tiếp xe chạy hay dừng và cần `dt` đều cho PID.
- **SensorTask chu kỳ 10 ms** (không phải 5 ms): `AGV_Encoder_Sample()` tính rpm từ số xung đếm được giữa hai lần gọi, gọi càng dày thì mỗi lần càng ít xung, rpm càng nhiễu.
- **SonarTask, ModeTask cùng P3**: việc ngắn, không cần đúng hạn tới mức ms.
- **CommTask P2**: phản hồi lệnh chậm vài ms người dùng không nhận ra.
- **UiTask thấp nhất**: vẽ OLED qua I2C mất khoảng 20–25 ms, phải để mọi task khác chen ngang được.

---

## 3. Đối tượng IPC và bit sự kiện

### 3.1. Đối tượng giao tiếp

| Tên | Loại | Kích thước | Gửi | Nhận |
|---|---|---|---|---|
| `qSensor` | Queue (mailbox) | 1 × `SensorData_t` | SensorTask | ControlTask, CommTask, UiTask |
| `qRange` | Queue (mailbox) | 1 × `RangeData_t` | SonarTask | ControlTask, CommTask, UiTask |
| `qCtrl` | Queue (mailbox) | 1 × `CtrlOut_t` | ControlTask | CommTask, UiTask |
| `qMode` | Queue | 4 × `ModeCmd_t` | CommTask | ModeTask |
| `semEcho` | Binary semaphore | – | ISR EXTI8 | SonarTask |
| `sbUartRx` | Stream buffer | 64 byte | ISR USART1 | CommTask |
| `mtxParams` | Mutex | – | CommTask (ghi) | ControlTask (đọc) |
| `egSystem` | Event group | 24 bit | Nhiều task | Nhiều task |

### 3.2. Các bit trong `egSystem`

| Bit | Tên | Ai đặt / xóa | Ý nghĩa |
|---|---|---|---|
| 0 | `EVT_OBSTACLE` | SonarTask | Vật cản gần hơn ngưỡng (15 cm ở LINE, 8 cm ở FOLLOW). Xóa khi xa hơn 20 cm (hoặc Echo có lên nhưng không có vọng về) trong 3 lần đo liên tiếp |
| 1 | `EVT_LINE_LOST` | SensorTask | `AGV_Line_Read().valid == false` (cả 5 mắt không thấy line) |
| 2 | `EVT_ESTOP` | ModeTask | Dừng khẩn do người dùng gửi lệnh `S` |
| 4 | `MODE_IDLE` | ModeTask | Chỉ một bit `MODE_*` được bật tại một thời điểm |
| 5 | `MODE_LINE` | ModeTask | |
| 6 | `MODE_FOLLOW` | ModeTask | |
| 7 | `MODE_STOP` | ModeTask | Dừng tạm vì vật cản, tự chạy lại khi đường thông |

Quy tắc an toàn trong ControlTask: **nếu không có bit `MODE_LINE` hoặc `MODE_FOLLOW`, hoặc có `EVT_OBSTACLE` / `EVT_ESTOP`, hoặc `qSensor` cũ hơn 50 ms, hoặc `qRange` cũ hơn 150 ms, thì gọi `tb6612_stop()`.**

---

## 4. Các chế độ hoạt động

```
                 lệnh M1                         EVT_OBSTACLE
     ┌──────┐ ───────────────► ┌──────┐ ──────────────────────────► ┌──────┐
     │ IDLE │                  │ LINE │                              │ STOP │
     └──────┘ ◄─────────────── └──────┘ ◄────────────────────────── └──────┘
       ▲  │   M0, S, mất line > 2 s       đường thông liên tục ≥ 1 s
       │  │ lệnh M2           ┌────────┐
       │  └─────────────────► │ FOLLOW │
       └───────────────────── └────────┘
             M0, S
```

| Chế độ | Xe làm gì | LED / còi |
|---|---|---|
| IDLE | Đứng yên, chờ lệnh | LED xanh nháy 1 Hz |
| LINE | Bám line: `control_line_step()` | LED xanh sáng liên tục |
| FOLLOW | Bám vật: `control_follow_step()` | LED xanh nháy 5 Hz |
| STOP | Dừng vì vật cản, servo quét 45° – 90° – 135° | LED đỏ, còi kêu ngắt quãng |

Chi tiết: [`04_may_trang_thai.md`](04_may_trang_thai.md).

---

## 5. Diễn biến và kịch bản

### KB0. Khởi động hệ thống

1. `main()` (CubeMX sinh): clock 72 MHz, GPIO (kèm EXTI6/7/8), TIM1, TIM2, TIM3, I2C2, USART1.
2. `MX_FREERTOS_Init()` gọi `app_rtos_init()`:
   - Tạo queue, mutex, semaphore, event group, stream buffer.
   - `AGV_Sensors_Init(&htim2, &htim1, CPWR)`: chạy TIM2 đếm µs, servo về 90°, xóa bộ đếm encoder.
   - `control_init()`: khởi tạo 4 bộ PID. `tb6612_init()`: bật PWM TIM3, STBY = 1, PWM = 0.
   - Bật nhận UART bằng ngắt, đặt `egSystem = MODE_IDLE`, tạo 6 task.
3. `osKernelStart()`: scheduler chạy, ControlTask (P5) chạy trước rồi lần lượt các task thấp hơn.
4. UiTask hiện `Mode: IDLE`, LED xanh nháy 1 Hz. CommTask gửi `AGV READY`.

### KB1. Bám line bình thường

**Điều kiện:** chế độ LINE, xe nằm trên line, không có vật cản.

| Thời điểm | Sự kiện |
|---|---|
| Liên tục | Mỗi lỗ đĩa encoder đi qua → ngắt EXTI6/EXTI7 → `AGV_Encoder_OnEXTI()` tăng bộ đếm |
| 0 ms | **ControlTask (P5)** thức dậy: copy tham số (giữ mutex vài µs), peek `qSensor`, đọc `egSystem` → có `MODE_LINE` → đổi `line.error` (float) sang `int8_t`, gọi `control_line_step()` → `tb6612_set_motor()` → ghi `qCtrl` → Blocked |
| khoảng 0,3 ms | **SensorTask (P4)**: `AGV_Line_Read()`, `AGV_Encoder_Sample()`, tính rpm trên cửa sổ 100 ms → `xQueueOverwrite(qSensor)` → Blocked |
| còn lại | Task thấp hơn chạy nếu tới lượt, phần còn lại là Idle |
| 10 ms | Lặp lại |

UiTask (P1) vẽ OLED mỗi 200 ms và liên tục bị ControlTask, SensorTask chen ngang. CommTask gửi một dòng telemetry mỗi 100 ms.

### KB2. Gặp vật cản khi đang bám line

1. SonarTask (P3) gọi `AGV_Sonar_Poll()` (dọn trạng thái cũ) → `AGV_Sonar_Trigger()` (xung Trig 10 µs) → `xSemaphoreTake(semEcho, 35 ms)` → Blocked.
2. Echo lên: ngắt EXTI8 → `AGV_Sonar_OnEXTI()` ghi thời điểm cạnh lên theo TIM2.
3. Echo xuống: ngắt EXTI8 → driver ghi cạnh xuống, trạng thái `READY` → callback chung thấy Echo ở mức thấp → `xSemaphoreGiveFromISR(semEcho)` → `portYIELD_FROM_ISR`.
4. SonarTask sang Ready, chờ tới lượt (P3) → `AGV_Sonar_Poll()` → 12 cm < 15 cm → set `EVT_OBSTACLE`.
5. **ControlTask ở chu kỳ 10 ms kế tiếp** thấy `EVT_OBSTACLE` → `control_reset_all()` → `tb6612_stop()`. Bước này **không phụ thuộc ModeTask**.
6. ModeTask (tối đa 20 ms sau) chuyển **LINE → STOP**. SonarTask bắt đầu quét servo 90° → 45° → 90° → 135°, mỗi vị trí giữ 300 ms để servo ổn định.
7. UiTask bật LED đỏ, còi kêu, OLED hiện `STOP`.

**Độ trễ xấu nhất từ lúc vật xuất hiện tới lúc PWM = 0:** khoảng 60 ms (chờ chu kỳ đo) + tối đa 25 ms (sóng bay) + 10 ms (chu kỳ ControlTask) ≈ **95 ms**. Ở 0,3 m/s xe đi thêm khoảng 3 cm.

**Khi vật được nhấc ra:** SonarTask chỉ đánh giá khi servo nhìn thẳng (90°) và đã đứng yên. Sau 3 lần đo > 20 cm thì xóa `EVT_OBSTACLE` (khoảng 1,2 s vì servo đang quét). ModeTask thấy đường thông liên tục ≥ 1 s → **STOP → LINE**. ControlTask phát hiện đổi chế độ → `control_reset_all()` → xe chạy lại êm. Tổng cộng khoảng 2–2,5 s.

### KB3. Mất line

1. `AGV_Line_Read()` trả `valid = false` → SensorTask set `EVT_LINE_LOST`.
2. ControlTask truyền mã `99` vào `control_line_step()`. Code Mảng 2 dùng `last_valid_err` để quay về phía line vừa mất.
3. Tìm lại line trong 2 s → SensorTask xóa `EVT_LINE_LOST`, bám line tiếp.
4. Quá 2 s → ModeTask chuyển **LINE → IDLE**, ControlTask dừng xe, LED đỏ nháy.

### KB4. Đổi chế độ bằng điện thoại

1. Người dùng gửi `M1\n`. Mỗi byte → ngắt USART1 → `xStreamBufferSendFromISR(sbUartRx)`.
2. CommTask (P2) ghép tới `\n` → `xQueueSend(qMode, CMD_MODE_LINE)`.
3. ModeTask (P3, cao hơn CommTask) đang chờ `qMode` → **chiếm CPU ngay trong lời gọi `xQueueSend`** → xóa `MODE_IDLE`, đặt `MODE_LINE`.
4. CommTask chạy tiếp, trả lời `OK M1`.
5. ControlTask chu kỳ kế tiếp thấy chế độ đổi → `control_reset_all()` → bắt đầu bám line.

### KB5. Chỉnh hệ số PID khi xe đang chạy

1. Người dùng gửi `KP1 50`.
2. CommTask lấy `mtxParams` → ghi `gParams.kpLine = 50`, tăng `gParams.version` → trả mutex → `OK KP1=50.00`.
3. ControlTask chu kỳ sau copy `gParams`, thấy `version` đổi → gọi `control_set_gains()` (hàm của Mảng 2, xem [`02`](02_giao_dien_module.md)). Các bộ PID nằm trong `control.c` chỉ được ControlTask chạm vào, nên không cần khóa thêm.
4. **Đảo ưu tiên:** nếu CommTask (P2) đang giữ mutex đúng lúc ControlTask (P5) cần, FreeRTOS tạm nâng CommTask lên P5 để SonarTask/ModeTask (P3) không chen vào. ControlTask chỉ trễ vài µs.

### KB6. Bám vật thể

1. SonarTask giữ servo 90°, đo khoảng cách, đọc 2 FC-51 → `qRange`. Ngưỡng `EVT_OBSTACLE` giảm còn 8 cm.
2. ControlTask thấy `MODE_FOLLOW`:
   - Sonar **không hợp lệ** → dừng xe (RTOS tự xử lý, không gọi PID với khoảng cách 0).
   - Sonar hợp lệ → `control_follow_step(distance_cm, fcL, fcR, rpmL, rpmR, dt, …)`: PID khoảng cách ra V_base giữ 20 cm, FC-51 quyết định hướng rẽ, dưới 15 cm thì Mảng 2 tự dừng.
3. Vật áp sát dưới 8 cm → `EVT_OBSTACLE` → ControlTask dừng ngay, chế độ vẫn là FOLLOW.

### KB7. Siêu âm không phản hồi

1. Hết 35 ms không có Echo → `xSemaphoreTake` trả về timeout → `AGV_Sonar_Poll()` trả `valid = false`.
2. Không đặt `EVT_OBSTACLE`. Ở LINE xe vẫn chạy; ở FOLLOW xe đứng yên.
3. Đang ở STOP mà cảm biến không phản hồi (Echo không lên): `EVT_OBSTACLE` được giữ nguyên, xe không tự chạy lại. Echo có lên nhưng không có vọng về thì vẫn tính là đường thông.
4. Nhờ timeout, SonarTask không bao giờ kẹt vô hạn.

### KB8. Dừng khẩn bằng điện thoại

1. Gửi `S`. CommTask → `qMode` → ModeTask đặt `EVT_ESTOP`, chuyển IDLE.
2. ControlTask chu kỳ kế tiếp → `tb6612_stop()`.
3. Gửi `M1` hoặc `M2` để chạy lại (ModeTask xóa `EVT_ESTOP`).

### KB9. Lỗi phần mềm: tràn stack

1. FreeRTOS phát hiện (`configCHECK_FOR_STACK_OVERFLOW = 2`) → `vApplicationStackOverflowHook()`.
2. Hook gọi `tb6612_stop()`, bật LED đỏ, dừng hệ thống.
3. Debug: đọc `pcTaskName` để biết task nào tràn, tăng stack cho task đó.

---

## 6. Phân tích thời gian

### 6.1. Tải CPU

**U = Σ (Cᵢ / Tᵢ)**, C là thời gian chạy một lần, T là chu kỳ. Số ước lượng; số đo thật lấy theo [`06_kiem_thu_va_do_dac.md`](06_kiem_thu_va_do_dac.md).

| Task | C (ước lượng) | T | C/T | C đo thật |
|---|---|---|---|---|
| ControlTask | 0,3 ms | 10 ms | 3,0 % | |
| SensorTask | 0,1 ms | 10 ms | 1,0 % | |
| SonarTask | 0,2 ms | 60 ms | 0,3 % | |
| ModeTask | 0,02 ms | 20 ms | 0,1 % | |
| CommTask | 0,5 ms | 100 ms | 0,5 % | |
| UiTask (OLED) | 25 ms | 200 ms | 12,5 % | |
| Ngắt EXTI encoder | 2 µs × khoảng 130 lần/s | – | < 0,1 % | |
| **Tổng** | | | **≈ 17,5 %** | |

Giới hạn Liu & Layland của Rate Monotonic với 6 task ≈ **73 %**. Tổng 17,5 % thấp hơn nhiều, hệ thống lập lịch được.

### 6.2. Độ trễ phản hồi

| Sự kiện | Độ trễ xấu nhất | Thành phần |
|---|---|---|
| Vật cản → PWM = 0 | ≈ 95 ms | Chu kỳ sonar 60 ms + sóng bay 25 ms + ControlTask 10 ms |
| Lệnh đổi chế độ → xe phản ứng | ≈ 10 ms | Chu kỳ ControlTask |
| Lệnh chỉnh tham số → có hiệu lực | ≈ 10 ms | Như trên |
| Thay đổi line → PID phản ứng | ≈ 20 ms | SensorTask 10 ms + ControlTask 10 ms |

---

## 7. Ghép code của các mảng

| Hàm | Mảng | Ai gọi | Ghi chú |
|---|---|---|---|
| `AGV_Sensors_Init(&htim2, &htim1, cpwr)` | 1 | `app_rtos_init` | Một lần lúc khởi động |
| `AGV_Sensors_OnEXTI(pin)` | 1 | `HAL_GPIO_EXTI_Callback` (Mảng 3) | Trong ngắt |
| `AGV_Line_Read()` | 1 | SensorTask | `error` float −2…+2, bit 4 = mắt trái |
| `AGV_Encoder_Sample()` | 1 | **Chỉ SensorTask** | Có trạng thái bên trong |
| `AGV_Sonar_Poll()`, `AGV_Sonar_Trigger()` | 1 | SonarTask | |
| `AGV_Servo_SetAngle(&htim1, deg)` | 1 | SonarTask | 90° = nhìn thẳng |
| `AGV_FC51_Read()` | 1 | SonarTask | |
| `control_init()`, `control_reset_all()` | 2 | `app_rtos_init`, ControlTask | |
| `control_line_step(err, rpmL, rpmR, dt, &L, &R)` | 2 | ControlTask | `err` int8; `99` = mất line |
| `control_follow_step(dist, fcL, fcR, rpmL, rpmR, dt, &L, &R)` | 2 | ControlTask | Chỉ gọi khi sonar hợp lệ |
| `control_set_gains(...)` | 2 | ControlTask | Đổi hệ số khi đang chạy |
| `tb6612_init()`, `tb6612_set_motor()`, `tb6612_stop()` | 2 | `app_rtos_init`, ControlTask | **Chỉ ControlTask** gọi set/stop |

Chi tiết và các sửa đổi ở code Mảng 2: [`02_giao_dien_module.md`](02_giao_dien_module.md).

---

## 8. Cấu trúc thư mục project

```
AGV_Project/                         ← project STM32CubeIDE
├── Core/
│   ├── Inc/
│   │   ├── agv_*.h                  ← Mảng 1
│   │   ├── pid.h, control.h, tb6612.h   ← Mảng 2
│   │   ├── app_rtos.h, app_config.h     ← Mảng 3
│   │   ├── ssd1306.h, ssd1306_conf.h, ssd1306_fonts.h
│   │   └── FreeRTOSConfig.h         ← CubeMX sinh, thêm vài dòng
│   └── Src/
│       ├── main.c                   ← CubeMX sinh
│       ├── freertos.c               ← CubeMX sinh, gọi app_rtos_init()
│       ├── agv_*.c                  ← Mảng 1
│       ├── pid.c, control.c, tb6612.c   ← Mảng 2
│       ├── app_rtos.c, app_mode.c, app_comm.c, app_ui.c   ← Mảng 3
│       └── ssd1306.c, ssd1306_fonts.c
└── Middlewares/Third_Party/FreeRTOS/
```

---

## 9. Tài liệu chi tiết

| File | Nội dung |
|---|---|
| [`01_cau_hinh_cubemx.md`](01_cau_hinh_cubemx.md) | Tạo project, clock, chân, TIM1/2/3/4, EXTI, USART1, I2C2, FreeRTOS, NVIC |
| [`02_giao_dien_module.md`](02_giao_dien_module.md) | Kiểu dữ liệu trao đổi, quy ước dấu, cách ghép driver Mảng 1 và PID Mảng 2, **các sửa đổi ở Mảng 2** |
| [`03_trien_khai_task.md`](03_trien_khai_task.md) | Code `app_rtos.c`: đối tượng IPC, ControlTask, SensorTask, SonarTask, các callback ngắt, hook lỗi, UiTask |
| [`04_may_trang_thai.md`](04_may_trang_thai.md) | ModeTask: bảng chuyển trạng thái, code, bảng kiểm thử |
| [`05_giao_thuc_bluetooth.md`](05_giao_thuc_bluetooth.md) | Lệnh, telemetry, cấu hình HC-05, CommTask |
| [`06_kiem_thu_va_do_dac.md`](06_kiem_thu_va_do_dac.md) | Dựng hệ thống từng bước, kiểm thử kịch bản, đo thời gian và stack, bảng lỗi |

Sơ đồ chân toàn xe: `src/hardware/docs/Tut.md`, mục 1.2. Đấu dây nguồn, động cơ và mạch phụ: mục 1.3.
