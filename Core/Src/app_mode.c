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
