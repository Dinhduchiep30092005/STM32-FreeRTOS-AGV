# 01. Cấu hình project trên STM32CubeIDE

Từng bước tạo project, cấu hình clock, chân, ngoại vi và FreeRTOS theo **sơ đồ chân đã chốt** (`Tut.md`, mục 1.2). Mỗi bước có phần kiểm tra trước khi đi tiếp.

[← Về README](README.md)

---

## Bước 1. Tạo project

1. **File → New → STM32 Project** → tab **MCU/MPU Selector** → tìm `STM32F103C8T6` → **Next**.
2. Tên `AGV_Project`, ngôn ngữ C, kiểu **Executable** → **Finish**.
3. Mở `AGV_Project.ioc`.

---

## Bước 2. SYS và RCC

| Mục | Thiết lập | Lý do |
|---|---|---|
| **SYS → Debug** | `Serial Wire` | Nạp qua ST-Link; giải phóng PA15, PB3, PB4 |
| **SYS → Timebase Source** | **`TIM4`** | FreeRTOS dùng SysTick; `HAL_GetTick()` (driver Mảng 1 dùng nhiều) lấy nhịp từ TIM4 |
| **RCC → HSE** | `Crystal/Ceramic Resonator` | Thạch anh 8 MHz trên Blue Pill |

---

## Bước 3. Clock 72 MHz

| Mục | Giá trị |
|---|---|
| Input frequency | 8 MHz, PLL Source = HSE, PLLMul ×9 |
| SYSCLK / HCLK | 72 MHz |
| APB1 Prescaler | /2 → PCLK1 = 36 MHz, **APB1 timer clock = 72 MHz** |
| APB2 Prescaler | /1 → **APB2 timer clock = 72 MHz** |

TIM1 (APB2) và TIM2, TIM3, TIM4 (APB1) đều chạy 72 MHz.

---

## Bước 4. Chân

Bảng đầy đủ ở `Tut.md` mục 1.2. Cấu hình trong CubeMX:

| Chân | Chế độ | User Label |
|---|---|---|
| PA0 … PA4 | `GPIO_Input` | `LINE0` … `LINE4` |
| PA6, PA7 | `TIM3_CH1`, `TIM3_CH2` | |
| PA8 | `TIM1_CH1` | |
| PA9, PA10 | `USART1_TX`, `USART1_RX` | |
| PA11 | `GPIO_Output` | `LED_GREEN` |
| PA12 | `GPIO_Output` | `LED_RED` |
| PA15 | `GPIO_Output` | `BUZZER` |
| PB0, PB1 | `GPIO_Input` | `FC_L`, `FC_R` |
| PB3, PB4 | `GPIO_Output` | `DBG1`, `DBG2` |
| PB5 | `GPIO_Output`, Output level **Low** | **`STBY`** |
| PB6, PB7 | **`GPIO_EXTI6`, `GPIO_EXTI7`** | `ENC_L`, `ENC_R` |
| PB8 | **`GPIO_EXTI8`** | `ECHO` |
| PB9 | `GPIO_Output`, Low | `TRIG` |
| PB10, PB11 | **`I2C2_SCL`, `I2C2_SDA`** | |
| PB12 … PB15 | `GPIO_Output`, Low | **`AIN1`, `AIN2`, `BIN1`, `BIN2`** |
| PC13 | `GPIO_Output` | `LED_BOARD` |

Các User Label in đậm **phải đặt đúng tên**, vì `tb6612.c` của Mảng 2 dùng macro `STBY_Pin`, `AIN1_GPIO_Port`… do CubeMX sinh từ label.

**GPIO → cấu hình EXTI:**

| Chân | GPIO mode | Pull |
|---|---|---|
| PB6, PB7 (encoder) | External Interrupt Mode with **Rising** edge trigger detection | No pull (hoặc Pull-up nếu tín hiệu FC-03 trôi) |
| PB8 (Echo) | External Interrupt Mode with **Rising/Falling** edge trigger detection | **Pull-down** (tuột dây Echo thì chân về 0, không nhiễu thành xung giả). Khi bật điện trở nội phải **giữ mạch chia áp 5 V → 3,3 V**, không nối thẳng Echo 5 V vào PB8 |

**Kiểm tra:** không chân nào bị tô vàng/đỏ.

---

## Bước 5. Timer

### TIM1: PWM servo 50 Hz (Mảng 1)

Clock Source `Internal Clock`, Channel1 `PWM Generation CH1`.

| Tham số | Giá trị |
|---|---|
| Prescaler | `71` (1 MHz) |
| Counter Period | `19999` (20 ms = 50 Hz) |
| Pulse CH1 | `1500` |
| Mode / Polarity | PWM mode 1 / High |

### TIM2: bộ đếm µs (Mảng 1)

Clock Source `Internal Clock`, **không bật kênh nào**.

| Tham số | Giá trị |
|---|---|
| Prescaler | `71` (1 MHz, mỗi đơn vị = 1 µs) |
| Counter Period | `65535` |

Không cần ngắt. `AGV_Sensors_Init()` gọi `HAL_TIM_Base_Start()`.

### TIM3: PWM động cơ (Mảng 2)

Clock Source `Internal Clock`, Channel1 và Channel2 `PWM Generation`.

| Tham số | Giá trị | Giải thích |
|---|---|---|
| Prescaler | `3` | 72 MHz / 4 = 18 MHz |
| Counter Period | `999` | 18 MHz / 1000 = **18 kHz**, gần ngoài vùng nghe, động cơ không rít |
| Pulse CH1, CH2 | `0` | |

ARR = 999 khớp với `MAX_PWM = 1000` trong `control.c`: PWM 1000 tương ứng 100 % chu kỳ.

### TIM4

Đã chọn làm timebase ở bước 2, CubeMX tự cấu hình. Không dùng cho việc khác.

---

## Bước 6. USART1 và I2C2

**USART1** (HC-05): Asynchronous, `9600` baud (hoặc `115200` sau khi cấu hình HC-05, xem [`05`](05_giao_thuc_bluetooth.md)), 8N1. **NVIC:** bật `USART1 global interrupt`.

**I2C2** (OLED): Mode `I2C`, Fast Mode, `400000` Hz. Không cần ngắt.

---

## Bước 7. FreeRTOS

**Middleware → FREERTOS**: Interface `CMSIS_V2`.

| Tham số | Giá trị | Giải thích |
|---|---|---|
| `USE_PREEMPTION` | Enabled | Lập lịch có chiếm quyền |
| `TICK_RATE_HZ` | `1000` | 1 tick = 1 ms |
| `MAX_PRIORITIES` | `56` (mặc định) | Đủ cho ưu tiên 1…5 |
| `TOTAL_HEAP_SIZE` | `10240` | Tổng stack 6 task khoảng 5,5 KB + đối tượng IPC + dự phòng |
| `USE_MUTEXES` | Enabled | |
| `CHECK_FOR_STACK_OVERFLOW` | `Option2` | |
| `USE_MALLOC_FAILED_HOOK` | Enabled | |
| `USE_TRACE_FACILITY` | Enabled | Cho lệnh `STK` |
| `GENERATE_RUN_TIME_STATS` | Enabled khi đo, Disabled khi chạy thật | Xem [`06`](06_kiem_thu_va_do_dac.md) |
| `LIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | `5` | |

Heap: `heap_4`. Tab **Tasks and Queues**: giữ `defaultTask`, không tạo gì thêm. Mọi task và đối tượng tạo bằng code trong `app_rtos_init()`.

> **Cạm bẫy:** `defaultTask` có ưu tiên `osPriorityNormal` = **24**, cao hơn mọi task 1…5 của mình. Phải cho nó tự xóa (bước 9).

---

## Bước 8. NVIC

| Ngắt | Preemption Priority | Gọi API FreeRTOS? |
|---|---|---|
| **EXTI line[9:5]** (PB6, PB7, PB8) | **`5`** | Có (`xSemaphoreGiveFromISR`) |
| USART1 global interrupt | `6` | Có (`xStreamBufferSendFromISR`) |
| TIM4 (timebase HAL) | `15` | Không |
| SysTick, PendSV | `15` | FreeRTOS quản lý |

> **Quy tắc:** trên Cortex-M, số NVIC **càng nhỏ càng ưu tiên cao** (ngược FreeRTOS). Ngắt có ưu tiên 0–4 **không được gọi API FreeRTOS**, đặt sai là treo ngẫu nhiên.

Đặt EXTI = 5 cao hơn USART1 = 6 để ngắt Echo không bị trễ khi đang nhận byte UART, giữ sai số đo khoảng cách nhỏ.

---

## Bước 9. Sinh code, chép file và nối vào FreeRTOS

1. **Project Manager → Code Generator**: tích `Generate peripheral initialization as a pair of '.c/.h' files per peripheral`. **Ctrl + S** để sinh code.
2. Chép code các mảng vào project:

| Nguồn | Đích |
|---|---|
| `src/hardware/src/*.h` | `Core/Inc/` |
| `src/hardware/src/*.c` | `Core/Src/` |
| `src/pid/src/pid.h`, `control.h`, `tb6612.h` | `Core/Inc/` |
| `src/pid/src/pid.c`, `control.c`, `tb6612.c` | `Core/Src/` |
| `src/pid/src/test_control_pc.c` | **Không chép**, đây là chương trình test trên máy tính, sẽ trùng `main()` |
| Code Mảng 3 (`app_*.c/.h`), thư viện SSD1306 | `Core/Inc/`, `Core/Src/` |

3. Mở `Core/Src/freertos.c`, thêm vào các vùng `USER CODE`:

```c
/* USER CODE BEGIN Includes */
#include "app_rtos.h"
/* USER CODE END Includes */

void MX_FREERTOS_Init(void) {
  /* ... CubeMX sinh defaultTask ... */
  /* USER CODE BEGIN RTOS_THREADS */
  app_rtos_init();
  /* USER CODE END RTOS_THREADS */
}

void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  vTaskDelete(NULL);   /* ưu tiên 24, xóa ngay */
  /* USER CODE END StartDefaultTask */
}
```

4. CubeMX tự sinh `HAL_GPIO_EXTI_Callback` rỗng dạng `__weak` trong HAL, không cần xóa. Hàm thật nằm trong `app_rtos.c`. **Không được có `HAL_GPIO_EXTI_Callback` thứ hai** ở `main.c` hay file của Mảng 1.

---

## Bước 10. Kiểm tra

Tạm thời viết `app_rtos_init()` chỉ tạo một task nháy LED PC13 mỗi 500 ms (`vTaskDelay`), nạp và chạy.

| Kiểm tra | Mong đợi |
|---|---|
| LED PC13 | Nháy đúng 1 Hz |
| Sinh code | Không còn cảnh báo "SysTick is used as timebase" |
| Build | 0 error, RAM dùng dưới 16 KB (Build Analyzer) |

Đạt cả ba thì chuyển sang [`02_giao_dien_module.md`](02_giao_dien_module.md).
