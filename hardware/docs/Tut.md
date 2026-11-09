# AGV Mảng 1 — Driver cảm biến STM32F103C8T6 sử dụng HAL
5 cảm biến dò line số; 2 encoder FC-03 một kênh; HC-SR04 đo xung Echo kết hợp servo SG90; 2 cảm biến hồng ngoại FC-51.

## 1. Sơ đồ chân

### 1.1. Chân của Mảng 1 (cảm biến)

| Tín hiệu | Chân | Cấu hình CubeMX | Điện áp cấp cho module |
|---|---|---|---|
| Dò line TCRT 5 mắt, tín hiệu số | PA0 (S0, trái nhất), PA1, PA2, PA3, PA4 (S4, phải nhất) | GPIO Input (chọn pull-up/pull-down sau khi kiểm tra đầu ra module) | **3,3 V** (PA0–PA4 không chịu được 5 V) |
| Encoder FC-03 trái/phải | PB6, PB7 | GPIO_EXTI6, EXTI7, cạnh lên (Rising edge); bật ngắt EXTI9_5 | 3,3 V hoặc 5 V (PB6, PB7 chịu được 5 V) |
| FC-51 trái/phải | PB0, PB1 | GPIO Input | **3,3 V** (PB0, PB1 không chịu được 5 V) |
| HC-SR04 TRIG | PB9 | GPIO Output Push-Pull; mặc định mức thấp | 5 V |
| HC-SR04 ECHO | PB8 | GPIO_EXTI8, cả hai cạnh; bật ngắt EXTI9_5 | 5 V. **Bắt buộc giữ mạch chia áp 5 V → 3,3 V** vì PB8 bật Pull-down nội (xem mục 1.3), không nối thẳng |
| Tín hiệu servo SG90 | PA8 | TIM1_CH1, PWM Generation CH1 | 5 V từ LM2596 |
| Timer đo thời gian micro giây | TIM2 | Internal Clock; bộ đếm 1 MHz, ARR = 65535, không xuất chân | – |
| Bộ định thời PWM servo | TIM1 | Bộ đếm 1 MHz, ARR = 19999; CCR1 từ 1000–2000 tick (xung khoảng 1–2 ms, tần số 50 Hz) | – |

### 1.2. Sơ đồ chân toàn xe (đã thống nhất với Mảng 2 và Mảng 3)

Giữ nguyên toàn bộ chân của Mảng 1. Các khối còn lại xếp vào chân trống. **OLED chuyển sang I2C2 (PB10, PB11)** vì I2C1 mặc định dùng PB6/PB7, trùng với encoder.

| Chân | Nối với | Chế độ CubeMX | Mảng |
|---|---|---|---|
| PA0–PA4 | Dò line S0…S4 | GPIO Input | 1 |
| PA5 | Dự phòng | – | |
| PA6 | PWMA (TB6612) | TIM3_CH1, PWM 18 kHz | 2 |
| PA7 | PWMB (TB6612) | TIM3_CH2, PWM 18 kHz | 2 |
| PA8 | Servo SG90 | TIM1_CH1, PWM 50 Hz | 1 |
| PA9 | RXD của HC-05 | USART1_TX | 3 |
| PA10 | TXD của HC-05 | USART1_RX | 3 |
| PA11 | LED xanh (qua 220 Ω) | GPIO Output | 3 |
| PA12 | LED đỏ (qua 220 Ω) | GPIO Output | 3 |
| PA13, PA14 | ST-Link (SWDIO, SWCLK) | SYS → Debug = Serial Wire | – |
| PA15 | Còi (qua 1 kΩ → transistor S8050) | GPIO Output | 3 |
| PB0, PB1 | FC-51 trái, phải | GPIO Input | 1 |
| PB3, PB4 | Chân đo thời gian (debug) | GPIO Output | 3 |
| PB5 | STBY (TB6612) | GPIO Output, mặc định Low | 2 |
| PB6, PB7 | Encoder trái, phải | GPIO_EXTI6, EXTI7 | 1 |
| PB8 | Echo HC-SR04 | GPIO_EXTI8, cả hai cạnh | 1 |
| PB9 | Trig HC-SR04 | GPIO Output | 1 |
| PB10 | OLED SCL | **I2C2_SCL** | 3 |
| PB11 | OLED SDA | **I2C2_SDA** | 3 |
| PB12, PB13 | AIN1, AIN2 (TB6612) | GPIO Output | 2 |
| PB14, PB15 | BIN1, BIN2 (TB6612) | GPIO Output | 2 |
| PC13 | LED có sẵn trên board | GPIO Output | 3 |

Phân chia timer:

| Timer | Dùng cho |
|---|---|
| TIM1 | PWM servo 50 Hz (PA8) |
| TIM2 | Đếm µs tự do, đo Echo và encoder (không xuất chân) |
| TIM3 | PWM động cơ 18 kHz (PA6, PA7; PSC = 3, ARR = 999, khớp MAX_PWM = 1000 của Mảng 2) |
| TIM4 | **Timebase của HAL** (SYS → Timebase Source = TIM4), vì FreeRTOS dùng SysTick |

Lưu ý: PA11, PA12 là chân USB, không bật USB trong CubeMX. PA15, PB3, PB4 chỉ dùng được khi chọn Debug = Serial Wire.

### 1.3. Đấu dây đầy đủ (nguồn, động cơ, mạch phụ)

Mục 1.2 cho biết chân tín hiệu của STM32. Mục này bổ sung phần nguồn, động cơ và các mạch phụ. Ô `___` là thông số nhóm điền theo linh kiện thực tế.

**Nguồn.** Một khay 4 viên 18650, ghép 2 nối tiếp × 2 song song (2S2P): 7,4 V danh định, đầy 8,4 V.

> **Đo trước khi nối:** lắp đủ 4 pin, đo hai dây ra của khay. Kết quả phải trong khoảng 6–8,4 V. Nhiều khay 4 viên bán sẵn nối tiếp cả 4 viên: nếu đo ra 12–16,8 V thì **không được nối vào VM**, vì TB6612 chỉ chạy tới 13,5 V (tối đa tuyệt đối 15 V).

| Từ | Đến | Ghi chú |
|---|---|---|
| Khay pin (+) | Công tắc | |
| Công tắc | LM2596 IN+ và TB6612 VM | Tụ 470–1000 µF giữa VM và GND, đặt sát TB6612 |
| Khay pin (−) | LM2596 IN− và GND chung | |
| LM2596 OUT+ | Chân 5V Blue Pill, servo, HC-SR04, HC-05 | Chỉnh đúng 5,0 V **trước khi** nối tải |
| LM2596 OUT− | GND chung | |
| Chân 3.3 Blue Pill | Dò line, FC-51, TB6612 VCC | Dò line và FC-51 nối vào chân không chịu được 5 V |
| 3,3 V hoặc 5 V | Encoder FC-03 | PB6, PB7 chịu được 5 V |
| ___ V (theo nhãn module) | OLED VCC | |

**GND chung:** âm pin, LM2596, Blue Pill, TB6612 và mọi module phải nối với nhau.

**TB6612**

| Chân | Nối với | Ghi chú |
|---|---|---|
| VM | Sau công tắc | Điện áp định mức động cơ: ___ V, đối chiếu với 8,4 V khi pin đầy |
| VCC | 3,3 V từ Blue Pill | Không cấp 5 V: ngưỡng mức cao là 0,7 × VCC, tín hiệu 3,3 V của STM32 sẽ không đủ |
| GND | GND chung | Nối đủ các chân GND của module |
| STBY, PWMA, PWMB, AIN1, AIN2, BIN1, BIN2 | Xem mục 1.2 | |
| AO1, AO2 | Động cơ **trái** | Kênh A trong `tb6612.c` |
| BO1, BO2 | Động cơ **phải** | Kênh B trong `tb6612.c` |

Bánh nào quay ngược chiều thì đổi hai dây của động cơ đó, không sửa code.

**Mạch phụ**

| Mạch | Cách nối | Giá trị |
|---|---|---|
| Chia áp Echo | Echo → R1 → PB8; PB8 → R2 → GND | R2 / (R1 + R2) ≈ 2/3. R1 = ___, R2 = ___; R2 nên nhỏ hơn nhiều so với pull-down nội (khoảng 40 kΩ) |
| LED xanh, đỏ | PA11, PA12 → 220 Ω → anode LED; cathode → GND | Mức cao = sáng |
| Còi | PA15 → 1 kΩ → cực B S8050; E → GND; C → còi (−); còi (+) → ___ V | Mức cao = kêu |
| HC-05 | Xem `rtos/docs/05_giao_thuc_bluetooth.md`, mục 1 | |
| ST-Link | Khi xe đang cấp nguồn, chỉ nối SWDIO, SWCLK, GND | Không nối thêm 3,3 V của ST-Link |

**Cấp nguồn lần đầu:** (1) đo điện áp khay; (2) chưa nối tải, chỉnh LM2596 ra 5,0 V; (3) đo thông mạch để chắc 5 V, 3,3 V và VM không chập GND; (4) kê xe lên rồi mới bật công tắc.

## 2. Cấu hình clock và Timer trong CubeMX

Sau `SystemClock_Config`, CubeMX xác định clock Timer dựa trên bộ chia APB. Trên STM32F1, clock Timer có thể bằng **2 lần PCLK** nếu prescaler APB khác 1. Kiểm tra `TIM1_CLK` và `TIM2_CLK` **Clock Configuration**.

Thiết lập:
- `PSC = (TIMx_CLK / 1000000) - 1` để mỗi lần đếm tương ứng 1 µs (yêu cầu phép chia nguyên).
- **TIM2:** `ARR = 65535`; khởi động Timer cơ sở trong `AGV_Sensors_Init`.
- **TIM1:** `ARR = 19999`; TIM1 CH1 dùng PWM mode 1, cực tính High; chân PA8 là Alternate-Function Push-Pull; bật đầu ra TIM1 theo mã do HAL tạo.
- **TIM4:** chọn làm timebase của HAL (SYS → Timebase Source = TIM4). FreeRTOS dùng SysTick, nên `HAL_GetTick()` lấy nhịp từ TIM4.
- **NVIC:** bật `EXTI9_5_IRQn`; trong `EXTI9_5_IRQHandler()` gọi `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);`, tiếp theo PIN_7 và PIN_8 (CubeMX có thể tự tạo các lệnh này).
- **Mức ưu tiên NVIC:** `EXTI9_5` = **5**, USART1 = 6, TIM4 (timebase) = 15. Ngắt nào gọi API FreeRTOS phải có mức ưu tiên ≥ 5 (`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`).
- Trong toàn bộ firmware chỉ có **một** `HAL_GPIO_EXTI_Callback`, do **Mảng 3 (RTOS)** viết trong `app_rtos.c`. Callback này gọi `AGV_Sensors_OnEXTI(pin)` trước, sau đó khi Echo xuống mức thấp thì `xSemaphoreGiveFromISR(semEcho)` để đánh thức SonarTask. Các hàm driver của Mảng 1 vẫn **không gọi API RTOS**.

```c
void HAL_GPIO_EXTI_Callback(uint16_t pin)          /* duy nhất, trong app_rtos.c */
{
    AGV_Sensors_OnEXTI(pin);                        /* driver đếm encoder, đo Echo */
    if (pin == AGV_ECHO_PIN &&
        HAL_GPIO_ReadPin(AGV_ECHO_PORT, AGV_ECHO_PIN) == GPIO_PIN_RESET) {
        BaseType_t w = pdFALSE;
        xSemaphoreGiveFromISR(semEcho, &w);
        portYIELD_FROM_ISR(w);
    }
}
```

## 3. Quy ước dữ liệu đầu ra

### 3.1. `AGV_Line_Read()`

- Bit 4 = cảm biến trái S0; bit 0 = cảm biến phải S4.
- Sau chuẩn hóa bằng `AGV_LINE_ACTIVE`, giá trị **1** nghĩa là cảm biến phát hiện line.
- Sai số tính theo trung bình trọng số:

  `error = Σ(weight_i × hit_i) / Σ(hit_i)` với trọng số `−2, −1, 0, +1, +2`.

- **Quy ước dấu dùng chung cả nhóm:** `error < 0` là line nằm bên **trái** xe, `error > 0` là line nằm bên **phải** xe. Khi `error > 0`, xe phải rẽ phải nên bánh trái quay nhanh hơn: `v_target_L = V_base + u`, `v_target_R = V_base − u` (Mảng 2).

- Nếu không mắt nào phát hiện line: `valid = false`. Giá trị `error = 0` lúc này chỉ là **giá trị thay thế**, **KHÔNG được hiểu là xe đang ở giữa line**.
- Nếu cả 5 mắt đều phát hiện line, có thể xe đang ở một giao điểm; thuật toán quản lý chế độ của nhóm sẽ quyết định cách xử lý.

### 3.2. `AGV_Encoder_Sample()` — Encoder

- Trả về tổng số xung đếm được (`left_count/right_count`), số xung tăng thêm trong kỳ lấy mẫu và **độ lớn RPM không có dấu**.
- Cần hiệu chuẩn `counts_per_wheel_rev` bằng cách quay bánh xe thật, có xét tỉ số truyền và việc đếm theo cạnh xung.
- **FC-03 một kênh không thể tự xác định chiều quay!**
- Nếu lấy mẫu mỗi 10 ms, RPM có thể nhiễu ở tốc độ thấp. Có thể cân nhắc làm mượt trong cửa sổ 50–100 ms, nhưng vẫn giữ nhịp cập nhật Task 10 ms của nhóm.

### 3.3. `AGV_Sonar_Trigger()`, `AGV_Sonar_OnEXTI()`, `AGV_Sonar_Poll()` — Siêu âm

- `AGV_Sonar_Trigger()` phát xung TRIG 10 µs; không sử dụng delay chặn RTOS. Tần suất mục tiêu tối đa khoảng **mỗi 60 ms một lần**.
- `AGV_Sonar_OnEXTI()` đo hai thời điểm cạnh xung bằng TIM2 (bộ đếm quay vòng sau 65,536 ms).
- `AGV_Sonar_Poll()` coi khoảng độ rộng xung từ 100 đến 30000 µs là hợp lệ; tính `distance_cm ≈ pulse_us / 58`. Sau **35 ms** không nhận kết quả, phép đo được đánh dấu không hợp lệ do timeout.
- Không phát TRIG mới trước khi lần đo trước hoàn tất hoặc đã timeout.
- Chuỗi quét servo phải tự chờ servo ổn định sau khi quay (thường ít nhất 150–300 ms; cần đo thực tế). **Chu kỳ SonarTask 60 ms không có nghĩa là quét xong ba góc trong 60 ms.**
- Phép trừ xử lý Timer quay vòng giả định một xung Echo dài dưới 65,536 ms; độ rộng xung hợp lệ tối đa ở đây là 30 ms.

### 3.4. `AGV_FC51_Read()` — Phát hiện vật cản

- Trả về hai cờ vật cản đã được chuẩn hóa.
- Mặc định coi tín hiệu kích hoạt là **active-low** (mức thấp nghĩa là phát hiện vật); phải kiểm tra cực tính và khoảng phát hiện thực tế của module.

### 3.5. `AGV_Servo_SetAngle()` — Điều khiển SG90

- Góc danh định 0–180° ánh xạ thành độ rộng xung 1–2 ms.
- Hành trình thực tế và các điểm giới hạn của SG90 có thể khác nhau giữa từng servo.
- Nên bắt đầu thử ở 60°, 90° và 120°, sau đó hiệu chuẩn để tránh ép servo vào điểm chặn cơ khí.

## 4. Quy trình kiểm thử

1. **Tắt nguồn toàn bộ động cơ:** xác minh bảng chân, không chập mạch, kiểm tra điện áp đầu ra LM2596 và GND chung.
2. **Dò line:** in ra 5 bit và sai số khi di chuyển cảm biến trên nền đen/trắng. Điều chỉnh `AGV_LINE_ACTIVE` cho đúng cực tính.
3. **Encoder:** quay bánh xe bằng tay 10 vòng; lấy chênh lệch số đếm chia 10 để hiệu chuẩn số count/vòng bánh (CPWR). Kiểm tra nhãn trái/phải. Không suy ra chiều quay có dấu từ một cảm biến FC-03 một kênh.
4. **FC-51:** đưa vật cản tới từng phía, kiểm tra module kích hoạt ở mức thấp hay mức cao.
5. **Servo:** đặt 90°, sau đó 60° và 120°. Kiểm tra dạng PWM và nguồn 5 V ổn định, cấp riêng phù hợp cho servo.
6. **HC-SR04:** so sánh kết quả đo tại 10, 20 và 30 cm bằng thước. Kiểm tra mạch hạ mức ECHO và xử lý timeout; nếu cần, thực hiện lọc trung bình hoặc trung vị ở tầng xử lý cao hơn.
7. **Tích hợp với nhóm RTOS:** thống nhất thời hạn hợp lệ của dữ liệu, cơ chế khóa/bảo vệ dữ liệu và tránh trùng callback. Kiểm thử dừng khẩn cấp bằng nhánh an toàn riêng do nhóm 2/3 quản lý.

## 5. Đưa file vào dự án

- Chép `Core/Inc/agv_*.h` vào `Core/Inc/` của dự án STM32CubeIDE.
- Chép `Core/Src/agv_*.c` vào `Core/Src/` của dự án.
- Kiểm tra đường dẫn include tại **Project > Properties**.
- Ví dụ gọi hàm: xem `Docoutput.txt` ở thư mục `hardware/docs`. (File `Examples/main_user_code.c.txt` hiện chưa có.)
- Bộ này **không bao gồm** các hàm khởi tạo phần cứng sinh tự động từ CubeMX.
- Tên hàm và tên driver được đặt tiền tố riêng để hạn chế xung đột với mã của nhóm.

## 6. Lưu ý

- Sử dụng ngắt ngoài để đếm xung FC-03 ở tần số cao có thể làm CPU bận đáng kể. Chế độ Timer Encoder phần cứng cần hai tín hiệu quadrature A/B, nhưng FC-03 một kênh không có đủ hai tín hiệu đó. Nếu xung nhanh, nên cân nhắc Timer External Clock hoặc Input Capture.
- Đo Echo bằng EXTI có sai số do độ trễ phục vụ ngắt. Nếu yêu cầu chính xác cao, nên dùng Input Capture trên một kênh Timer chuyên dụng.
- Bộ này **chưa có** lọc cảm biến tự động, mạch chuyển mức 5 V được lắp sẵn, code buzzer/LED/OLED/HC-05 hoặc tính phần trăm pin. Các hạng mục này thuộc thành viên khác hoặc giai đoạn tích hợp sau.
- Code sử dụng `HAL_GetTick()` để tính thời gian mili giây. Đã giải quyết bằng cách chọn **TIM4 làm timebase của HAL** (mục 2), FreeRTOS giữ SysTick.
- `AGV_Encoder_Sample()` có biến trạng thái bên trong (`prev_*`), nên **chỉ một task được gọi** (SensorTask của Mảng 3).
- Đề xuất cải tiến encoder: trong `AGV_Encoder_OnEXTI` lưu thêm thời điểm theo TIM2 để tính **chu kỳ giữa 2 xung** (`rpm = 60e6 / (period_us × CPWR)`) và bỏ các xung cách nhau dưới khoảng 1 ms (nhiễu LM393). Cách này cho tốc độ mượt ở chu kỳ 10 ms, thay cho việc đếm xung vốn chỉ được 0–1 xung mỗi 10 ms.
