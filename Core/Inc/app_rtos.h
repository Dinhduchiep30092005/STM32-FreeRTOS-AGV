#ifndef APP_RTOS_H
#define APP_RTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "stream_buffer.h"

#include "agv_sensor_system.h"   /* Mảng 1 */
#include "control.h"             /* Mảng 2 */
#include "tb6612.h"              /* Mảng 2 */

/* ----- Bit trong egSystem ----- */
#define EVT_OBSTACLE    (1u << 0)
#define EVT_LINE_LOST   (1u << 1)
#define EVT_ESTOP       (1u << 2)
#define MODE_IDLE       (1u << 4)
#define MODE_LINE       (1u << 5)
#define MODE_FOLLOW     (1u << 6)
#define MODE_STOP       (1u << 7)
#define MODE_MASK       (MODE_IDLE | MODE_LINE | MODE_FOLLOW | MODE_STOP)

typedef enum { CMD_MODE_IDLE = 0, CMD_MODE_LINE, CMD_MODE_FOLLOW, CMD_ESTOP } ModeCmd_t;

/* ----- Kiểu dữ liệu (giải thích ở tài liệu 02, mục 2) ----- */
typedef struct { AGV_LineSample line; float rpmL, rpmR; uint32_t tick; } SensorData_t;
typedef struct { AGV_SonarSample sonar; AGV_FC51Sample fc; uint8_t servoDeg; uint32_t tick; } RangeData_t;
typedef struct { int16_t pwmL, pwmR; int8_t errSent; uint32_t tick; } CtrlOut_t;
typedef struct { float kpLine, kdLine, kpSpd, kiSpd, kpDist, vBase; uint32_t version; } Params_t;

/* ----- Đối tượng dùng chung ----- */
extern QueueHandle_t        qSensor, qRange, qCtrl, qMode;
extern SemaphoreHandle_t    semEcho, mtxParams;
extern EventGroupHandle_t   egSystem;
extern StreamBufferHandle_t sbUartRx;
extern Params_t             gParams;   /* chỉ truy cập khi giữ mtxParams */

void ModeTask(void *arg);   /* app_mode.c */
void CommTask(void *arg);   /* app_comm.c */
void UiTask(void *arg);     /* app_ui.c   */

/* ----- Chân đo thời gian PB3, PB4 (ghi thanh ghi trực tiếp, khoảng 1 chu kỳ máy) ----- */
#define DBG1_ON()   (GPIOB->BSRR = GPIO_PIN_3)
#define DBG1_OFF()  (GPIOB->BRR  = GPIO_PIN_3)
#define DBG2_ON()   (GPIOB->BSRR = GPIO_PIN_4)
#define DBG2_OFF()  (GPIOB->BRR  = GPIO_PIN_4)

void app_rtos_init(void);

#endif
