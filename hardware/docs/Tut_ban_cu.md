# AGV Mảng 1 — Driver cảm biến STM32F103C8T6 sử dụng HAL
5 cảm biến dò line số; 2 encoder FC-03 một kênh; HC-SR04 đo xung Echo kết hợp servo SG90; 2 cảm biến hồng ngoại FC-51.

## 1. Sơ đồ chân
| Dò line TCRT 5 mắt, tín hiệu số | PA0, PA1, PA2, PA3, PA4 | GPIO Input (chọn pull-up/pull-down sau khi kiểm tra đầu ra module) |
| Encoder FC-03 trái/phải | PB6, PB7 | GPIO_EXTI6, EXTI7, cạnh lên (Rising edge); bật ngắt EXTI9_5 |
| FC-51 trái/phải | PB0, PB1 | GPIO Input |
| HC-SR04 TRIG | PB9 | GPIO Output Push-Pull; mặc định mức thấp |
| HC-SR04 ECHO | PB8 | GPIO_EXTI8, cả hai cạnh; bật ngắt EXTI9_5; **hạ mức tín hiệu 5 V → 3,3 V** |
| Tín hiệu servo SG90 | PA8 | TIM1_CH1, PWM Generation CH1 |
| Timer đo thời gian micro giây | TIM2 | Internal Clock; bộ đếm 1 MHz, ARR = 65535, không xuất chân |
| Bộ định thời PWM servo | TIM1 | Bộ đếm 1 MHz, ARR = 19999; CCR1 từ 1000–2000 tick (xung khoảng 1–2 ms, tần số 50 Hz) |

## 3. Cấu hình clock và Timer trong CubeMX

Sau `SystemClock_Config`, CubeMX xác định clock Timer dựa trên bộ chia APB. Trên STM32F1, clock Timer có thể bằng **2 lần PCLK** nếu prescaler APB khác 1. Kiểm tra `TIM1_CLK` và `TIM2_CLK` **Clock Configuration**.

Thiết lập:
- `PSC = (TIMx_CLK / 1000000) - 1` để mỗi lần đếm tương ứng 1 µs (yêu cầu phép chia nguyên).
- **TIM2:** `ARR = 65535`; khởi động Timer cơ sở trong `AGV_Sensors_Init`.
- **TIM1:** `ARR = 19999`; TIM1 CH1 dùng PWM mode 1, cực tính High; chân PA8 là Alternate-Function Push-Pull; bật đầu ra TIM1 theo mã do HAL tạo.
- **NVIC:** bật `EXTI9_5_IRQn`; trong `EXTI9_5_IRQHandler()` gọi `HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);`, tiếp theo PIN_7 và PIN_8 (CubeMX có thể tự tạo các lệnh này).
- Trong toàn bộ firmware, chỉ nên có **một** `HAL_GPIO_EXTI_Callback`; từ đó chuyển xử lý sang `AGV_Sensors_OnEXTI`.
- Khi dùng FreeRTOS, callback này **không gọi API RTOS**. Nếu sau này gọi `xQueueSendFromISR` hoặc `xSemaphoreGiveFromISR`, cần kiểm tra mức ưu tiên ngắt so với `configMAX_SYSCALL_INTERRUPT_PRIORITY`.

## 4. Quy ước dữ liệu đầu ra 

### 4.1. `AGV_Line_Read()`

- Bit 4 = cảm biến trái S0; bit 0 = cảm biến phải S4.
- Sau chuẩn hóa bằng `AGV_LINE_ACTIVE`, giá trị **1** nghĩa là cảm biến phát hiện line.
- Sai số tính theo trung bình trọng số:

  `error = Σ(weight_i × hit_i) / Σ(hit_i)` với trọng số `−2, −1, 0, +1, +2`.

- Nếu không mắt nào phát hiện line: `valid = false`. Giá trị `error = 0` lúc này chỉ là **giá trị thay thế**, **KHÔNG được hiểu là xe đang ở giữa line**.
- Nếu cả 5 mắt đều phát hiện line, có thể xe đang ở một giao điểm; thuật toán quản lý chế độ của nhóm sẽ quyết định cách xử lý.

### 4.2. `AGV_Encoder_Sample()` — Encoder

- Trả về tổng số xung đếm được (`left_count/right_count`), số xung tăng thêm trong kỳ lấy mẫu và **độ lớn RPM không có dấu**.
- Cần hiệu chuẩn `counts_per_wheel_rev` bằng cách quay bánh xe thật, có xét tỉ số truyền và việc đếm theo cạnh xung.
- **FC-03 một kênh không thể tự xác định chiều quay!**
- Nếu lấy mẫu mỗi 10 ms, RPM có thể nhiễu ở tốc độ thấp. Có thể cân nhắc làm mượt trong cửa sổ 50–100 ms, nhưng vẫn giữ nhịp cập nhật Task 10 ms của nhóm.

### 4.3. `AGV_Sonar_Trigger()`, `AGV_Sonar_OnEXTI()`, `AGV_Sonar_Poll()` — Siêu âm

- `AGV_Sonar_Trigger()` phát xung TRIG 10 µs; không sử dụng delay chặn RTOS. Tần suất mục tiêu tối đa khoảng **mỗi 60 ms một lần**.
- `AGV_Sonar_OnEXTI()` đo hai thời điểm cạnh xung bằng TIM2 (bộ đếm quay vòng sau 65,536 ms).
- `AGV_Sonar_Poll()` coi khoảng độ rộng xung từ 100 đến 30000 µs là hợp lệ; tính `distance_cm ≈ pulse_us / 58`. Sau **35 ms** không nhận kết quả, phép đo được đánh dấu không hợp lệ do timeout.
- Không phát TRIG mới trước khi lần đo trước hoàn tất hoặc đã timeout.
- Chuỗi quét servo phải tự chờ servo ổn định sau khi quay (thường ít nhất 150–300 ms; cần đo thực tế). **Chu kỳ SonarTask 60 ms không có nghĩa là quét xong ba góc trong 60 ms.**
- Phép trừ xử lý Timer quay vòng giả định một xung Echo dài dưới 65,536 ms; độ rộng xung hợp lệ tối đa ở đây là 30 ms.

### 4.4. `AGV_FC51_Read()` — Phát hiện vật cản

- Trả về hai cờ vật cản đã được chuẩn hóa.
- Mặc định coi tín hiệu kích hoạt là **active-low** (mức thấp nghĩa là phát hiện vật); phải kiểm tra cực tính và khoảng phát hiện thực tế của module.

### 4.5. `AGV_Servo_SetAngle()` — Điều khiển SG90

- Góc danh định 0–180° ánh xạ thành độ rộng xung 1–2 ms.
- Hành trình thực tế và các điểm giới hạn của SG90 có thể khác nhau giữa từng servo.
- Nên bắt đầu thử ở 60°, 90° và 120°, sau đó hiệu chuẩn để tránh ép servo vào điểm chặn cơ khí.

## Quy trình kiểm thử

1. **Tắt nguồn toàn bộ động cơ:** xác minh bảng chân, không chập mạch, kiểm tra điện áp đầu ra LM2596 và GND chung.
2. **Dò line:** in ra 5 bit và sai số khi di chuyển cảm biến trên nền đen/trắng. Điều chỉnh `AGV_LINE_ACTIVE` cho đúng cực tính.
3. **Encoder:** quay bánh xe bằng tay 10 vòng; lấy chênh lệch số đếm chia 10 để hiệu chuẩn số count/vòng bánh (CPWR). Kiểm tra nhãn trái/phải. Không suy ra chiều quay có dấu từ một cảm biến FC-03 một kênh.
4. **FC-51:** đưa vật cản tới từng phía, kiểm tra module kích hoạt ở mức thấp hay mức cao.
5. **Servo:** đặt 90°, sau đó 60° và 120°. Kiểm tra dạng PWM và nguồn 5 V ổn định, cấp riêng phù hợp cho servo.
6. **HC-SR04:** so sánh kết quả đo tại 10, 20 và 30 cm bằng thước. Kiểm tra mạch hạ mức ECHO và xử lý timeout; nếu cần, thực hiện lọc trung bình hoặc trung vị ở tầng xử lý cao hơn.
7. **Tích hợp với nhóm RTOS:** thống nhất thời hạn hợp lệ của dữ liệu, cơ chế khóa/bảo vệ dữ liệu và tránh trùng callback. Kiểm thử dừng khẩn cấp bằng nhánh an toàn riêng do nhóm 2/3 quản lý.

## 7. Đưa file vào 

- Chép `hardware/src/*.h` vào thư mục header tương ứng của dự án STM32CubeIDE.
- Chép `hardware/src/*.c` vào thư mục mã nguồn tương ứng.
- Kiểm tra đường dẫn include tại **Project > Properties**.
- Xem ví dụ trong `Examples/main_user_code.c.txt`.
- Bộ này **không bao gồm** các hàm khởi tạo phần cứng sinh tự động từ CubeMX.
- Tên hàm và tên driver được đặt tiền tố riêng để hạn chế xung đột với mã của nhóm.

## 8. Noti

- Sử dụng ngắt ngoài để đếm xung FC-03 ở tần số cao có thể làm CPU bận đáng kể. Chế độ Timer Encoder phần cứng cần hai tín hiệu quadrature A/B, nhưng FC-03 một kênh không có đủ hai tín hiệu đó. Nếu xung nhanh, nên cân nhắc Timer External Clock hoặc Input Capture.
- Đo Echo bằng EXTI có sai số do độ trễ phục vụ ngắt. Nếu yêu cầu chính xác cao, nên dùng Input Capture trên một kênh Timer chuyên dụng.
- Bộ này **chưa có** lọc cảm biến tự động, mạch chuyển mức 5 V được lắp sẵn, code buzzer/LED/OLED/HC-05 hoặc tính phần trăm pin. Các hạng mục này thuộc thành viên khác hoặc giai đoạn tích hợp sau.
- Code sử dụng `HAL_GetTick()` để tính thời gian mili giây. Khi FreeRTOS sử dụng SysTick, cần bảo đảm cấu hình HAL tick tương thích.
