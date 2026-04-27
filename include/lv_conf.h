#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0
#define LV_USE_SYMBOL           1

/* Memory */
#define LV_MEM_SIZE             (128 * 1024U)
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN       4

/* Buffers */
#define LV_DRAW_BUF_SIZE        (480 * 480 * (LV_COLOR_DEPTH/8))

#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_INFO

/* Fonts */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_USE_FONT_SUBPX       0
#define LV_USE_FONT_COMPRESSED  0

/* Recommended for performance */
#define LV_USE_DRAW_SW          1
#define LV_DRAW_SW_COMPLEX      1

#endif