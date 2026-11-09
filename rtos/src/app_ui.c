#include <stdio.h>
#include "main.h"
#include "app_rtos.h"
#include "app_config.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#define LED_G(x) HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define LED_R(x) HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define BUZ(x)   HAL_GPIO_WritePin(BUZZER_GPIO_Port,    BUZZER_Pin,    (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

static const char *mode_name(EventBits_t ev)
{
    if (ev & MODE_LINE)   return "LINE";
    if (ev & MODE_FOLLOW) return "FOLLOW";
    if (ev & MODE_STOP)   return "STOP";
    return "IDLE";
}

void UiTask(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    uint32_t   n = 0;
    SensorData_t s; RangeData_t r; CtrlOut_t c;
    char line[22];

    ssd1306_Init();

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(UI_PERIOD_MS));
        n++;
        HAL_GPIO_TogglePin(LED_BOARD_GPIO_Port, LED_BOARD_Pin);   /* "còn sống" */
        EventBits_t ev = xEventGroupGetBits(egSystem);

        /* --- LED, còi: mỗi 100 ms --- */
        if (ev & (MODE_STOP | EVT_ESTOP))                  { LED_G(0); LED_R(1); BUZ((n % 4) < 2); }
        else if ((ev & MODE_IDLE) && (ev & EVT_LINE_LOST)) { LED_G(0); LED_R(n & 1); BUZ(0); }
        else if (ev & MODE_LINE)                           { LED_G(1); LED_R(0); BUZ(0); }
        else if (ev & MODE_FOLLOW)                         { LED_G(n & 1); LED_R(0); BUZ(0); }
        else                                               { LED_G((n % 10) < 5); LED_R(0); BUZ(0); }

        /* --- OLED: mỗi 200 ms --- */
        if (n & 1) continue;
        xQueuePeek(qSensor, &s, 0);
        xQueuePeek(qRange,  &r, 0);
        xQueuePeek(qCtrl,   &c, 0);

        ssd1306_Fill(Black);
        snprintf(line, sizeof line, "Mode: %s", mode_name(ev));
        ssd1306_SetCursor(0, 0);  ssd1306_WriteString(line, Font_7x10, White);

        /* bit 4 = mắt trái: in từ trái sang phải */
        snprintf(line, sizeof line, "Line %c%c%c%c%c e%+d",
                 (s.line.bits & 0x10) ? '#' : '.', (s.line.bits & 0x08) ? '#' : '.',
                 (s.line.bits & 0x04) ? '#' : '.', (s.line.bits & 0x02) ? '#' : '.',
                 (s.line.bits & 0x01) ? '#' : '.', (int)(s.line.error * 10));
        ssd1306_SetCursor(0, 12); ssd1306_WriteString(line, Font_7x10, White);

        snprintf(line, sizeof line, "rpm %3d/%3d", (int)s.rpmL, (int)s.rpmR);
        ssd1306_SetCursor(0, 24); ssd1306_WriteString(line, Font_7x10, White);

        snprintf(line, sizeof line, "PWM %4d/%4d", c.pwmL, c.pwmR);
        ssd1306_SetCursor(0, 36); ssd1306_WriteString(line, Font_7x10, White);

        if (r.sonar.valid) snprintf(line, sizeof line, "Dist %3d cm", (int)r.sonar.distance_cm);
        else               snprintf(line, sizeof line, "Dist  --");
        ssd1306_SetCursor(0, 48); ssd1306_WriteString(line, Font_7x10, White);

        ssd1306_UpdateScreen();   /* I2C blocking ~20 ms, task khác chen ngang thoải mái */
    }
}
