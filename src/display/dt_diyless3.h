// HW pins
#define DISP_DC    GFX_NOT_DEFINED
#define DISP_MOSI  GFX_NOT_DEFINED
#define DISP_CS    1
#define DISP_SCK   12
#define DISP_SDA   11
#define DISP_RST   43
#define DISP_DE    45
#define DISP_HSYNC 5
#define DISP_VSYNC 4
#define DISP_PCLK  21

// Backlight
#define BACKLIGHT_PIN     46
#define BACKLIGHT_FREQ    5000
#define BACKLIGHT_RES     8
#define BACKLIGHT_MIN     155
#define BACKLIGHT_MAX     255
#define BACKLIGHT_GAMMA   0.8f
#define BACKLIGHT_DEFAULT 85

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
#define HSYNC_POL    1
#define HFRONT_PORCH 50
#define HPULSE_WIDTH 10
#define HBACK_PORCH  40
#define VSYNC_POL    1
#define VFRONT_PORCH 20
#define VPULSE_WIDTH 10
#define VBACK_PORCH  30
#define PCLK_ACT_NEG 1
#define PREFER_SPEED 8000000L
#define BOUNCE       false 

// Display resolution
#define DISP_WIDTH  480
#define DISP_HEIGHT 480

// Touch GT911
#define TOUCH_SDA 17
#define TOUCH_SCL 18
#define TOUCH_INT 10
#define TOUCH_RST GFX_NOT_DEFINED
#define TOUCH_GT911_ROTATION ROTATION_INVERTED
