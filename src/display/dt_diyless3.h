// HW pins
#define DISP_DC    GFX_NOT_DEFINED
#define DISP_CS    1
#define DISP_SCK   12
#define DISP_MOSI  11
#define DISP_MISO  GFX_NOT_DEFINED
#define DISP_RST   43
#define DISP_DE    45
#define DISP_HSYNC 5
#define DISP_VSYNC 4
#define DISP_PCLK  21

// Data pins (RGB565, 16 bits)
#define DISP_R1 39
#define DISP_R2 40
#define DISP_R3 41
#define DISP_R4 42
#define DISP_R5 2
#define DISP_G0 0
#define DISP_G1 9
#define DISP_G2 14
#define DISP_G3 47
#define DISP_G4 48
#define DISP_G5 3
#define DISP_B1 6
#define DISP_B2 7
#define DISP_B3 15
#define DISP_B4 16
#define DISP_B5 8

// Display sync
#define DISP_HSYNC_POL    1
#define DISP_HFRONT_PORCH 50
#define DISP_HPULSE_WIDTH 10
#define DISP_HBACK_PORCH  40
#define DISP_VSYNC_POL    1
#define DISP_VFRONT_PORCH 20
#define DISP_VPULSE_WIDTH 10
#define DISP_VBACK_PORCH  30
#define DISP_PCLK_ACT_NEG 1
#define DISP_PREFER_SPEED 8000000L
#define DISP_BOUNCE       false 

// Display resolution
#define DISP_WIDTH  480
#define DISP_HEIGHT 480

// Display init
#define DISP_INIT_SEQ    st7701_type1_init_operations

// Backlight
#define BACKLIGHT_PIN     46
#define BACKLIGHT_FREQ    5000
#define BACKLIGHT_RES     8
#define BACKLIGHT_MIN     155
#define BACKLIGHT_MAX     255
#define BACKLIGHT_GAMMA   0.8f
#define BACKLIGHT_DEFAULT 85

// Touch
#define TOUCH_TYPE_GT911
#define TOUCH_SDA      17
#define TOUCH_SCL      18
#define TOUCH_INT      10
#define TOUCH_RST      GFX_NOT_DEFINED
#define TOUCH_ROTATION ROTATION_INVERTED

// Opentherm STM32
#define OT_BOOT_PIN    44
#define OT_RESET_PIN   13
#define OT_IN_PIN      12
#define OT_OUT_PIN     11
