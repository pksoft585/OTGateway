// HW pins
#define DISP_DC    GFX_NOT_DEFINED
#define DISP_CS    39
#define DISP_SCK   48
#define DISP_MOSI  47
#define DISP_MISO  GFX_NOT_DEFINED
#define DISP_RST   GFX_NOT_DEFINED
#define DISP_DE    18
#define DISP_HSYNC 16
#define DISP_VSYNC 17
#define DISP_PCLK  21

// Data pins (RGB565, 16 bits)
#define DISP_R1 11
#define DISP_R2 12
#define DISP_R3 13
#define DISP_R4 14
#define DISP_R5 0
#define DISP_G0 8
#define DISP_G1 20
#define DISP_G2 3
#define DISP_G3 46
#define DISP_G4 9
#define DISP_G5 10
#define DISP_B1 4
#define DISP_B2 5
#define DISP_B3 6
#define DISP_B4 7
#define DISP_B5 15

// Display sync
#define DISP_HSYNC_POL    1
#define DISP_HFRONT_PORCH 10
#define DISP_HPULSE_WIDTH 10
#define DISP_HBACK_PORCH  50
#define DISP_VSYNC_POL    1
#define DISP_VFRONT_PORCH 10
#define DISP_VPULSE_WIDTH 10
#define DISP_VBACK_PORCH  20
#define DISP_PCLK_ACT_NEG false
#define DISP_PREFER_SPEED 8000000L
#define DISP_BOUNCE       false

// Display resolution
#define DISP_WIDTH  480
#define DISP_HEIGHT 480

// Display init
#define DISP_INIT_SEQ    st7701_type9_init_operations

// Backlight
#define BACKLIGHT_PIN     38
#define BACKLIGHT_FREQ    2000
#define BACKLIGHT_RES     8
#define BACKLIGHT_MIN     10
#define BACKLIGHT_MAX     255
#define BACKLIGHT_GAMMA   0.8f
#define BACKLIGHT_DEFAULT 85

// Touch
#define TOUCH_TYPE_GT911
#define TOUCH_SDA      19
#define TOUCH_SCL      45
#define TOUCH_INT      GFX_NOT_DEFINED
#define TOUCH_RST      GFX_NOT_DEFINED
#define TOUCH_ROTATION ROTATION_INVERTED
