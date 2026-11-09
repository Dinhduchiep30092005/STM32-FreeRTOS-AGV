# 04. Máy trạng thái (ModeTask)

ModeTask quyết định xe đang ở chế độ nào. Nó **không điều khiển động cơ**: chỉ ghi chế độ hiện tại vào `egSystem`, ControlTask đọc bit đó để chọn nhánh điều khiển. Tách như vậy giúp logic chuyển chế độ và logic điều khiển không dính vào nhau, mỗi phần test riêng được.

[← Về README](README.md)

---

## 1. Các trạng thái

| Trạng thái | Bit trong `egSystem` | Xe làm gì | Thoát khi |
|---|---|---|---|
| **IDLE** | `MODE_IDLE` | Đứng yên, PWM = 0 | Nhận lệnh `M1` hoặc `M2` |
| **LINE** | `MODE_LINE` | Bám line bằng PID ghép tầng | Có vật cản, mất line quá lâu, lệnh `M0` / `M2` / `S` |
| **STOP** | `MODE_STOP` | Dừng vì vật cản, servo quét 90° → 45° → 90° → 135° | Đường thông liên tục 1 s, hoặc lệnh |
| **FOLLOW** | `MODE_FOLLOW` | Đi theo vật phía trước, giữ cách 20 cm | Lệnh `M0` / `M1` / `S` |

Tại mỗi thời điểm **chỉ có đúng một** bit `MODE_*` được bật. Riêng `EVT_ESTOP` là cờ phụ: bật khi người dùng dừng khẩn, chỉ xóa khi người dùng gửi lệnh chế độ mới.

---

## 2. Sơ đồ chuyển trạng thái

```
                         M1                          EVT_OBSTACLE
      ┌────────┐ ──────────────────► ┌────────┐ ─────────────────────► ┌────────┐
      │        │                     │        │                        │        │
      │  IDLE  │ ◄────────────────── │  LINE  │ ◄───────────────────── │  STOP  │
      │        │  M0 · S             │        │  đường thông ≥ 1 s     │        │
      │        │  mất line ≥ 2 s     │        │                        │        │
      └────────┘                     └────────┘                        └────────┘
        │   ▲                          │   ▲                              │
        │   │ M0 · S                   │   │ M1                           │ M0 · S → IDLE
     M2 │   │                       M2 │   │                              │ M2 → FOLLOW
        ▼   │                          ▼   │
      ┌────────┐ ◄─────────────────────────┘
      │ FOLLOW │
      └────────┘
```

---

## 3. Bảng chuyển trạng thái

Mỗi lần ModeTask thức dậy (có lệnh trong `qMode`, hoặc hết timeout 20 ms), nó duyệt bảng này từ trên xuống. **Lệnh từ người dùng được xét trước sự kiện tự động.**

| # | Trạng thái hiện tại | Điều kiện | Trạng thái mới | Hành động kèm theo |
|---|---|---|---|---|
| 1 | Bất kỳ | Lệnh `S` | IDLE | Đặt `EVT_ESTOP` |
| 2 | Bất kỳ | Lệnh `M0` | IDLE | Xóa `EVT_ESTOP` |
| 3 | Bất kỳ | Lệnh `M1` | LINE | Xóa `EVT_ESTOP` |
| 4 | Bất kỳ | Lệnh `M2` | FOLLOW | Xóa `EVT_ESTOP` |
| 5 | LINE | `EVT_OBSTACLE` bật | STOP | Ghi nhớ "chạy lại thì về LINE" |
| 6 | LINE | `EVT_LINE_LOST` bật liên tục ≥ 2 s | IDLE | |
| 7 | STOP | `EVT_OBSTACLE` tắt liên tục ≥ 1 s | LINE (chế độ đã ghi nhớ) | |
| 8 | Còn lại | | Giữ nguyên | |

Lưu ý:

- **FOLLOW không chuyển sang STOP khi có vật cản.** Ở chế độ này, vật phía trước chính là mục tiêu cần bám. Có hai lớp dừng nhưng chế độ vẫn là FOLLOW: `control_follow_step()` của Mảng 2 tự dừng khi vật dưới 15 cm, và ControlTask dừng khi `EVT_OBSTACLE` (dưới 8 cm).
- **ModeTask không chịu trách nhiệm an toàn.** Khi có vật cản, ControlTask tự dừng xe ngay ở chu kỳ kế tiếp nhờ đọc thẳng `EVT_OBSTACLE`. ModeTask chuyển sang STOP chỉ để quyết định khi nào chạy lại, quét servo và báo trạng thái. Như vậy dù ModeTask chậm hay lỗi thì xe vẫn dừng đúng lúc.

---

## 4. Chuyển trạng thái an toàn

Đổi chế độ cần **xóa bit cũ rồi đặt bit mới**, tức hai thao tác riêng biệt. ControlTask (ưu tiên cao hơn) có thể chen vào giữa và thấy **không có bit chế độ nào**. Thiết kế đã tính trước trường hợp này: ControlTask coi "không có `MODE_LINE` hay `MODE_FOLLOW`" là phải dừng. Trong khoảng khắc đó xe ở trạng thái an toàn, không bao giờ chạy theo chế độ sai.

Khi phát hiện chế độ đổi, ControlTask gọi `control_reset_all()` (Mảng 2) một lần để xóa tích phân và sai số cũ, tránh xe giật khi bắt đầu chế độ mới.

---

## 5. Code `app_mode.c`

```c
#include <stdbool.h>
#include "app_rtos.h"
#include "app_config.h"

static void enter_mode(EventBits_t m)
{
    xEventGroupClearBits(egSystem, MODE_MASK);   /* (1) xóa bit cũ       */
    xEventGroupSetBits  (egSystem, m);           /* (2) đặt bit mới      */
}

void ModeTask(void *arg)
{
    (void)arg;
    EventBits_t mode = MODE_IDLE;       /* app_rtos_init() đã đặt sẵn   */
    EventBits_t resumeMode = MODE_LINE; /* STOP xong thì quay về đâu     */
    bool        lostTiming = false, clearTiming = false;
    TickType_t  lostSince = 0, clearSince = 0;
    ModeCmd_t   cmd;

    for (;;) {
        /* Chờ lệnh tối đa 20 ms. Có lệnh thì thức dậy NGAY (ModeTask P3
         * cao hơn CommTask P2 nên chiếm CPU ngay trong lời gọi xQueueSend). */
        bool gotCmd = (xQueueReceive(qMode, &cmd,
                                     pdMS_TO_TICKS(MODE_WAIT_MS)) == pdTRUE);
        EventBits_t ev   = xEventGroupGetBits(egSystem);
        TickType_t  now  = xTaskGetTickCount();
        EventBits_t next = mode;

        /* ---------- Hàng 1-4: lệnh người dùng, ưu tiên trước ---------- */
        if (gotCmd) {
            switch (cmd) {
            case CMD_ESTOP:
                xEventGroupSetBits(egSystem, EVT_ESTOP);
                next = MODE_IDLE;
                break;
            case CMD_MODE_IDLE:
                xEventGroupClearBits(egSystem, EVT_ESTOP);
                next = MODE_IDLE;
                break;
            case CMD_MODE_LINE:
                xEventGroupClearBits(egSystem, EVT_ESTOP);
                next = MODE_LINE;
                break;
            case CMD_MODE_FOLLOW:
                xEventGroupClearBits(egSystem, EVT_ESTOP);
                next = MODE_FOLLOW;
                break;
            }
        }
        /* ---------- Hàng 5-7: sự kiện tự động ---------- */
        else if (mode == MODE_LINE) {
            if (ev & EVT_OBSTACLE) {                          /* hàng 5 */
                resumeMode = MODE_LINE;
                next = MODE_STOP;
            } else if (ev & EVT_LINE_LOST) {                  /* hàng 6 */
                if (!lostTiming) { lostTiming = true; lostSince = now; }
                else if (now - lostSince >= pdMS_TO_TICKS(LINE_LOST_IDLE_MS))
                    next = MODE_IDLE;
            } else {
                lostTiming = false;
            }
        }
        else if (mode == MODE_STOP) {                         /* hàng 7 */
            if (ev & EVT_OBSTACLE) {
                clearTiming = false;
            } else {
                if (!clearTiming) { clearTiming = true; clearSince = now; }
                else if (now - clearSince >= pdMS_TO_TICKS(STOP_RESUME_MS))
                    next = resumeMode;
            }
        }

        /* ---------- Thực hiện chuyển ---------- */
        if (next != mode) {
            enter_mode(next);
            mode = next;
            lostTiming = clearTiming = false;   /* bộ đếm thời gian làm lại */
        }
    }
}
```

Vì sao dùng `xQueueReceive` có timeout 20 ms thay vì chờ vô hạn: ModeTask cần vừa phản ứng ngay với lệnh, vừa định kỳ kiểm tra các điều kiện theo thời gian ("mất line ≥ 2 s", "đường thông ≥ 1 s"). Timeout cho phép làm cả hai trong một vòng lặp, không cần thêm software timer.

---

## 6. Kiểm thử máy trạng thái

Gửi lệnh qua điện thoại (xem [`05_giao_thuc_bluetooth.md`](05_giao_thuc_bluetooth.md)) và quan sát OLED. Có thể nhấc xe lên khỏi mặt đất để bánh quay tự do khi test.

| # | Thao tác | Kết quả mong đợi | Đạt |
|---|---|---|---|
| 1 | Bật nguồn | OLED `Mode: IDLE`, LED xanh nháy 1 Hz | ☐ |
| 2 | Gửi `M1` | `Mode: LINE`, LED xanh sáng, bánh quay | ☐ |
| 3 | Đưa tay trước siêu âm < 15 cm | Bánh dừng trong < 0,1 s, `Mode: STOP`, LED đỏ, còi kêu, servo quét | ☐ |
| 4 | Rút tay ra | Khoảng 2–2,5 s sau tự về `Mode: LINE`, servo về 90°, bánh quay lại | ☐ |
| 5 | Nhấc xe khỏi line (5 mắt đều trắng) | Sau 2 s về `Mode: IDLE`, LED đỏ nháy | ☐ |
| 6 | Gửi `M2` | `Mode: FOLLOW`, LED xanh nháy nhanh | ☐ |
| 7 | Ở FOLLOW, đưa tay cách 12 cm | Bánh dừng (Mảng 2 dừng dưới 15 cm), chế độ **vẫn là FOLLOW** | ☐ |
| 8 | Ở FOLLOW, đưa tay cách 5 cm | Bánh dừng do `EVT_OBSTACLE` (lớp an toàn RTOS), chế độ vẫn FOLLOW | ☐ |
| 9 | Gửi `S` khi đang chạy | Dừng ngay, `Mode: IDLE`, LED đỏ, còi kêu | ☐ |
| 10 | Sau `S`, gửi `M1` | Chạy lại bình thường | ☐ |
| 11 | Gửi `M1` khi đang có vật cản phía trước | Vào LINE rồi chuyển ngay sang STOP, bánh không quay | ☐ |
