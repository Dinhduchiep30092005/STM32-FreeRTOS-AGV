/**
 * Cấu hình thư viện stm32-ssd1306 (afiskon, MIT) cho xe AGV.
 * Nguồn: github.com/afiskon/stm32-ssd1306, commit 25bd2e9 (2025-07-01).
 * Lập từ ssd1306_conf_template.h: STM32F1, I2C2, chỉ font 7x10.
 */

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

/* Họ vi điều khiển: Blue Pill là STM32F103 */
#define STM32F1

/* Bus: OLED nối PB10 (SCL), PB11 (SDA), handle hi2c2 do CubeMX sinh */
#define SSD1306_USE_I2C
#define SSD1306_I2C_PORT        hi2c2
#define SSD1306_I2C_ADDR        (0x3C << 1)

/* Chỉ bật font app_ui.c dùng: các font còn lại tốn nhiều Flash (chip chỉ có 64 KB) */
#define SSD1306_INCLUDE_FONT_7x10

/* Mặc định 128 x 64. Bật dòng dưới nếu màn hình cần lật:
 * #define SSD1306_MIRROR_VERT
 * #define SSD1306_MIRROR_HORIZ
 */

#endif /* __SSD1306_CONF_H__ */
