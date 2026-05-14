#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

/* Basic */
#define LV_COLOR_DEPTH           16
#define LV_COLOR_16_SWAP         0
#define LV_USE_SYMBOL            1
#define LV_USE_PNG               1

/* Memory */
#define LV_MEM_SIZE              (64 * 1024U)
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN        4

/* Logging */
#define LV_USE_LOG               0
#define LV_LOG_LEVEL             LV_LOG_LEVEL_WARN

/* Fonts */
#define LV_FONT_MONTSERRAT_18    1
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_MONTSERRAT_36    1
#define LV_FONT_MONTSERRAT_48    1
#define LV_USE_FONT_SUBPX        0
#define LV_USE_FONT_COMPRESSED   1

/* Performance */
#define LV_USE_DRAW_SW           1
#define LV_DEF_REFR_PERIOD       16
#define LV_DRAW_SW_COMPLEX       1
#define LV_DRAW_SW_SHADOW_CACHE  1
#define LV_INDEV_DEF_READ_PERIOD 50

/* Tick */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

#endif
#endif