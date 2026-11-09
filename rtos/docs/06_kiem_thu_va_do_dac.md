# 06. Kiểm thử, đo đạc và gỡ lỗi

Thứ tự dựng hệ thống từng bước, cách kiểm thử các kịch bản trong README, cách đo số liệu thật cho báo cáo (thời gian chạy, tải CPU, độ trễ, stack) và bảng lỗi hay gặp.

[← Về README](README.md)

---

## 1. Dựng hệ thống từng bước

**Không lắp hết rồi mới chạy.** Mỗi bước thêm một phần, kiểm tra đạt rồi mới sang bước sau. Bước nào lỗi thì biết ngay lỗi nằm ở phần vừa thêm.

| Bước | Thêm vào | Cách kiểm tra | Tiêu chí đạt |
|---|---|---|---|
| 1 | FreeRTOS + 1 task nháy LED PC13 | Nhìn LED | Nháy đúng 1 Hz |
| 2 | 2 task nháy 2 LED với chu kỳ khác nhau (100 ms, 300 ms) | Nhìn LED | Hai LED nháy độc lập. Chứng minh đa nhiệm chạy được |
| 3 | Đối tượng IPC + CommTask + UART | Gửi `GET` từ điện thoại | Nhận đúng phản hồi |
| 4 | SensorTask + `AGV_Line_Read()` (Mảng 1) | Di line dưới cảm biến, xem `bits`, `err10` trong telemetry | Line bên phải → `err10` dương. Âm thì cắm lại thứ tự dây |
| 5 | Kiểm tra cực tính dò line, FC-51 | Đặt mắt dò line lên vạch đen | Bit tương ứng = 1. Sai thì đổi `AGV_LINE_ACTIVE` / `AGV_FC_ACTIVE` |
| 6 | `AGV_Encoder_Sample()` + callback EXTI | Quay bánh bằng tay, xem `rpmL`, `rpmR` | Quay nhanh → rpm tăng, đứng yên → về 0 sau khoảng 100 ms |
| 7 | SonarTask + callback EXTI8 | Đưa vật ở 10, 20, 30 cm | `distance_cm` khớp thước, sai lệch ≤ 1–2 cm (đo bằng EXTI có trễ phục vụ ngắt) |
| 8 | ModeTask | Gửi `M1`, `M2`, `S` | OLED và LED đổi đúng (bảng test ở [`04`](04_may_trang_thai.md)) |
| 9 | ControlTask + `control.c`, `tb6612.c` (Mảng 2, **đã sửa theo [`02`](02_giao_dien_module.md) mục 4**), **kê xe lên** | Gửi `M1` | Bánh quay tiến; line lệch phải → bánh trái nhanh hơn. Ngược lại là còn lỗi dấu |
| 10 | Đặt xe xuống line | Gửi `M1` | Xe bám line |
| 11 | UiTask + OLED | Nhìn màn hình | Hiển thị đúng, xe vẫn bám line mượt khi OLED cập nhật |
| 12 | Chạy toàn bộ kịch bản KB0–KB9 trong README | Mục 2 dưới đây | Tất cả đạt |

Ở bước 9, nhấc xe lên (kê dưới gầm) để bánh quay tự do. Nếu chiều quay sai hoặc PID có dấu sai, xe sẽ không lao ra khỏi bàn.

---

## 2. Bảng kiểm thử kịch bản

| Kịch bản | Thao tác | Tiêu chí đạt | Số đo thật | Đạt |
|---|---|---|---|---|
| KB0 Khởi động | Bật nguồn | `AGV READY`, OLED IDLE trong < 1 s | ___ ms | ☐ |
| KB1 Bám line | `M1`, chạy 3 vòng sân | Không ra khỏi line; chân PB3 chu kỳ 10 ms ± 0,1 ms | ___ ms | ☐ |
| KB2 Vật cản | Đặt hộp lên line khi xe đang chạy | Dừng trước vật ≥ 5 cm; tự chạy lại khoảng 2–2,5 s sau khi nhấc hộp | Độ trễ ___ ms | ☐ |
| KB3 Mất line | Nhấc xe khỏi line | Xoay tìm line, sau 2 s về IDLE | ___ s | ☐ |
| KB4 Đổi chế độ | Gửi `M1` khi IDLE | Bánh quay trong < 50 ms kể từ lúc nhận lệnh | ___ ms | ☐ |
| KB5 Chỉnh Kp | Gửi `KP1 40` khi đang chạy | Xe đổi hành vi ngay, không giật, không reset | – | ☐ |
| KB6 Bám vật | `M2`, đi tay trước xe | Giữ khoảng 20 ± 5 cm | ___ cm | ☐ |
| KB7 Sonar lỗi | Rút dây Echo khi đang ở LINE; lặp lại khi đang ở STOP vì vật cản | LINE: xe vẫn bám line, không treo. STOP: xe **không** tự chạy lại | – | ☐ |
| KB8 Dừng khẩn | Gửi `S` | Dừng trong < 50 ms | ___ ms | ☐ |
| KB9 Tràn stack | Tạm giảm `STACK_COMM` còn 64, gửi `STK` | LED đỏ sáng, động cơ dừng | – | ☐ |

Sau KB9 nhớ trả `STACK_COMM` về giá trị cũ.

---

## 3. Đo thời gian chạy của task bằng chân GPIO

Cách trực quan nhất, rất hợp để chụp ảnh đưa vào báo cáo.

**Chuẩn bị:** logic analyzer giá rẻ (loại 8 kênh 24 MHz, phần mềm PulseView hoặc Saleae Logic) hoặc oscilloscope.

**Cách làm:**
1. Bật chân ở đầu mỗi lần chạy của task, tắt ở cuối. Code ở [`03`](03_trien_khai_task.md) đã có `DBG1_ON()` / `DBG1_OFF()` (PB3) trong ControlTask.
2. Gắn thêm `DBG2` (PB4) vào SensorTask hoặc UiTask. Cần thêm kênh thì dùng tạm PC13.
3. Nối các chân vào logic analyzer, lấy mẫu ≥ 1 MHz, ghi khoảng 1 giây.

**Đọc kết quả:**

| Đo | Trên hình | Ý nghĩa |
|---|---|---|
| Chu kỳ ControlTask | Khoảng cách hai cạnh lên liên tiếp của PB3 | Phải đúng 10,0 ms. Lệch nhiều là có task ưu tiên cao hơn hoặc ngắt quá dài |
| Thời gian chạy C | Độ rộng xung PB3 | Điền vào cột "C đo thật" ở README mục 6.1 |
| Jitter | Độ dao động của chu kỳ | Càng nhỏ càng tốt, mong đợi < 50 µs |
| Preemption | Xung PB4 (UiTask) bị **cắt làm nhiều đoạn**, mỗi khoảng trống trùng đúng với một xung PB3 | Hình ảnh chứng minh trực tiếp cơ chế chiếm quyền |

Chụp màn hình phần mềm logic analyzer với kênh PB3 (ControlTask) và PB4 (UiTask): xung dài của UiTask bị các xung ngắn của ControlTask chen ngang. Đây là hình minh họa preemption tốt nhất cho báo cáo.

---

## 4. Đo độ trễ phản hồi vật cản

**Mục tiêu:** đo thời gian từ lúc xung Echo báo vật cản tới lúc PWM về 0.

1. Nối kênh 1 của logic analyzer vào **chân Echo (PB8)**.
2. Nối kênh 2 vào **PA6** (PWMA ra động cơ trái).
3. Cho xe chạy LINE, đặt vật cản.
4. Trên hình: tìm xung Echo **ngắn bất thường** đầu tiên (vật gần, xung < 870 µs ứng với < 15 cm). Đo từ cạnh xuống của xung đó tới lúc PA6 ngừng phát xung PWM.

Giá trị lý thuyết: phần trễ phần mềm (sau khi Echo kết thúc) tối đa khoảng 10 ms + thời gian SonarTask chờ tới lượt. Ghi lại khoảng 10 lần đo, lấy giá trị lớn nhất và trung bình.

---

## 5. Đo tải CPU bằng run-time stats

FreeRTOS có thể đếm tổng thời gian mỗi task được chạy. Cần một bộ đếm nhanh hơn tick khoảng 10–100 lần. Cortex-M3 có sẵn **bộ đếm chu kỳ DWT**, không tốn timer nào.

**1. Bật trong CubeMX:** `GENERATE_RUN_TIME_STATS` = Enabled, `USE_STATS_FORMATTING_FUNCTIONS` = Enabled.

Code ở bước 3 và 4 đã có sẵn trong `Core/Src`, chỉ được biên dịch khi bật hai tùy chọn trên.

**2. Thêm vào `FreeRTOSConfig.h`** (trong vùng `USER CODE BEGIN Defines`):

```c
extern void     dwt_init(void);
extern uint32_t dwt_get_us(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  dwt_init()
#define portGET_RUN_TIME_COUNTER_VALUE()          dwt_get_us()
```

**3. Thêm vào `app_rtos.c`:**

```c
void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* bật khối trace   */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;              /* bật đếm chu kỳ    */
}

uint32_t dwt_get_us(void)
{
    return DWT->CYCCNT / 72;                           /* 72 MHz -> µs      */
}
```

Bộ đếm 32 bit ở 72 MHz tràn sau khoảng 59 giây, nên đọc thống kê trong vòng một phút sau khởi động, hoặc đọc 2 lần rồi lấy hiệu.

**4. Thêm lệnh `CPU` vào `handle_line()` của CommTask:**

```c
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
```

Kết quả dạng:

```
Control   18350    3%
Sensor     6120    1%
Sonar      2010   <1%
Mode        640   <1%
Comm       3900   <1%
Ui        71300   12%
IDLE     497680   82%
```

Tải CPU thực = 100 % − % của IDLE. Đưa bảng này vào báo cáo cạnh bảng ước lượng lý thuyết. Tắt `GENERATE_RUN_TIME_STATS` khi chạy thật để bớt tốn tài nguyên.

---

## 6. Đo và chỉnh kích thước stack

1. Cho xe chạy đủ mọi chế độ khoảng 1–2 phút (để mỗi task đi qua nhánh code tốn stack nhất).
2. Gửi `STK`. Kết quả là **số word còn trống ít nhất** từ lúc khởi động (high water mark):

```
STK Control 142
STK Sensor 71
STK Sonar 88
STK Mode 79
STK Comm 96
STK Ui 61
STK IDLE 98
```

3. Quy tắc chỉnh: giữ lại khoảng **20–30 % dự phòng**. Ví dụ ControlTask cấp 256, còn trống 142, tức dùng 114 → có thể giảm xuống khoảng 160. Task còn trống dưới 30 word thì phải tăng.
4. Ghi bảng (cấp phát / đã dùng / còn trống) vào báo cáo.

---

## 7. Bảng lỗi hay gặp

| Hiện tượng | Nguyên nhân thường gặp | Cách xử lý |
|---|---|---|
| Không task nào chạy, chỉ có `defaultTask` | `defaultTask` ưu tiên 24 chạy vòng lặp vô hạn | Gọi `vTaskDelete(NULL)` trong `StartDefaultTask` |
| Treo ngay khi khởi động, dừng ở `configASSERT` | Thiếu heap khi tạo task/queue | Tăng `TOTAL_HEAP_SIZE`, hoặc giảm stack |
| Treo ngẫu nhiên, thường khi có ngắt | Ngắt gọi API FreeRTOS có ưu tiên NVIC 0–4 | Đặt ưu tiên NVIC ≥ 5 (xem [`01`](01_cau_hinh_cubemx.md), bước 8) |
| HardFault | Tràn stack, con trỏ sai, gọi hàm không có `FromISR` trong ngắt | Bật kiểm tra tràn stack, xem `STK`; trong ISR chỉ dùng hàm `...FromISR` |
| Chu kỳ task không đều | Dùng `vTaskDelay` thay vì `vTaskDelayUntil`; hoặc có task ưu tiên cao chạy quá lâu | Đổi sang `vTaskDelayUntil`; đo bằng chân GPIO |
| Task thấp hơn không bao giờ chạy | Task ưu tiên cao dùng `HAL_Delay()` hoặc `while` chờ cờ | Thay bằng `vTaskDelay` / chờ semaphore |
| `distance_cm` luôn không hợp lệ | PB8 chưa đặt EXTI cả hai cạnh; EXTI9_5 chưa bật trong NVIC; có `HAL_GPIO_EXTI_Callback` thứ hai đè lên | Kiểm tra bước 4, 8 của [`01`](01_cau_hinh_cubemx.md); tìm trong project chỉ được có một callback |
| rpm nhảy loạn | Đếm xung trong cửa sổ ngắn; nhiễu LM393 ở FC-03 | Tăng `ENC_WIN`; dùng đo chu kỳ xung ([`02`](02_giao_dien_module.md) mục 5); hàn tụ 100 nF từ DO xuống GND |
| Xe rẽ ra xa line | Dấu vòng ngoài trong `control.c` ngược | Sửa theo [`02`](02_giao_dien_module.md) mục 4, việc 1 |
| Ở FOLLOW xe lùi khi vật ở xa | Dấu PID khoảng cách ngược | Việc 2 |
| Mất line là một bánh quay lùi hết tốc độ | Tốc độ mục tiêu âm nhưng rpm không dấu | Việc 3 |
| Servo giật hoặc động cơ không chạy | `tb6612.c` vẫn dùng `htim1` (trùng servo) | Việc 4: đổi sang `htim3` |
| Nhận được vài lệnh rồi UART im lặng | Lỗi overrun, không bật lại nhận; hoặc ngắt RX gặp `HAL_BUSY` vì TX đang giữ khóa HAL | Có `HAL_UART_ErrorCallback` (xem [`03`](03_trien_khai_task.md), mục 7); gửi qua `comm_send` có critical section (xem [`05`](05_giao_thuc_bluetooth.md), mục 6) |
| Không nhận được gì qua Bluetooth | Nối TX–TX; sai baud; app không gửi `\n` | TX nối RX; đúng baud với HC-05; bật Newline = LF trong app |
| OLED đứng hình nhưng xe vẫn chạy | UiTask bị chặn ở I2C (dây lỏng) | Kiểm tra dây SDA/SCL; đặt timeout cho hàm I2C. Xe vẫn chạy chính là nhờ UiTask ưu tiên thấp nhất |
| Xe giật mạnh khi chạy lại sau dừng | Tích phân PID không được reset | Kiểm tra `control_reset_all()` được gọi trong nhánh dừng và khi đổi chế độ |
| Xe chạy được một lúc thì reset | Sụt áp pin khi động cơ khởi động | Thêm tụ 470–1000 µF ở VM của TB6612; sạc pin; kiểm tra đầu ra LM2596 |
| `printf` / `snprintf` làm treo | In số thực `%f` hoặc stack quá nhỏ | Chỉ in số nguyên; tăng stack task có `snprintf` |

---

## 8. Số liệu nên đưa vào báo cáo

| Mục | Lấy từ |
|---|---|
| Bảng task: ưu tiên, chu kỳ, stack cấp phát / đã dùng | README mục 2 + lệnh `STK` |
| Bảng tải CPU: lý thuyết và đo thật | README mục 6.1 + lệnh `CPU` |
| Hình logic analyzer chứng minh preemption | Mục 3 |
| Độ trễ phản hồi vật cản: lý thuyết và đo thật | README mục 6.2 + mục 4 |
| Sơ đồ máy trạng thái và bảng kiểm thử | [`04`](04_may_trang_thai.md) |
| Đồ thị `err`, `rpm` theo thời gian từ telemetry | Mục 5 của [`05`](05_giao_thuc_bluetooth.md) |
| Bảng kết quả 10 kịch bản | Mục 2 |
