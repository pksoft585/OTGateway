#pragma once

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "dt_init.h"

#if defined(DISPLAY_TYPE_DIYLESS3)
#include "dt_diyless3.h"
// LVGL buffer
#define LV_BUF_ROWS 40 
#endif

#if defined(DISPLAY_TYPE_GUITION)
#include "dt_guition.h"
// LVGL buffer
#define LV_BUF_ROWS 40
static lv_color_t *lv_buf2 = nullptr;
#endif

#if defined(TOUCH_TYPE_GT911)
#include <TAMC_GT911.h>
#endif

#if defined(TOUCH_TYPE_FT6X36)
#include <FT6X36.h>
#endif

#if defined(TOUCH_TYPE_CST816)
#include <cst816t.h>
#endif

#if defined(DISPLAY_AHT20)
#include <Adafruit_AHTX0.h>
#endif

#if defined(DISPLAY_SPLASH_SCREEN)
LV_IMG_DECLARE(ot_icon_boot);
#endif

// DisplayTask Interval & Task priority
#define DISPLAYTASK_INTERVAL 33
const UBaseType_t ACTIVE_PRIORITY = 5;
const UBaseType_t SLEEP_PRIORITY  = 1;

// AHT20 sensor
#if defined(DISPLAY_AHT20)
Adafruit_AHTX0 aht20;
bool dt_aht20 = false;
#endif

// Global display objects
Arduino_DataBus *bus = nullptr;
Arduino_ESP32RGBPanel *rgbpanel = nullptr;
Arduino_RGB_Display *gfx = nullptr;
lv_display_t *lv_display = nullptr;
static lv_color_t *lv_buf1 = nullptr;
volatile bool displayGrayscaleMode = false;

// LGVL Tick timer
esp_timer_handle_t lvgl_tick_timer = nullptr;

// Forward declarations
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
void SplashScreen();

// Touch structure
template <typename T>
struct TouchStructure
{
    int16_t x = 0;
    int16_t y = 0;
    bool pressed = false;
    bool last_state = false;
    bool blocked = true;
    bool wait_release = false;

    T* dev = nullptr;
};

#if defined(TOUCH_TYPE_GT911)
    TouchStructure<TAMC_GT911> touch;
#endif

#if defined(TOUCH_TYPE_FT6X36)
    TouchStructure<FT6X36> touch;
#endif

#if defined(TOUCH_TYPE_CST816)
    TouchStructure<cst816t> touch;
#endif

// Backlight functions
void initBacklight()
{
    ledcAttach(BACKLIGHT_PIN, BACKLIGHT_FREQ, BACKLIGHT_RES);
    ledcWrite(BACKLIGHT_PIN, 0);
}

void setBacklight(uint8_t brightness)
{
    if (brightness > 100)
    {
        brightness = 100;
    }
    float normalized = brightness / 100.0f;
    float gamma = powf(normalized, BACKLIGHT_GAMMA);
    float range = BACKLIGHT_MAX - BACKLIGHT_MIN;
    uint8_t output = (uint8_t)(BACKLIGHT_MIN + gamma * range + 0.5f);
    ledcWrite(BACKLIGHT_PIN, output);
}

void offBacklight()
{
    ledcWrite(BACKLIGHT_PIN, 0);
}

// Set grayscale mode
void setDisplayGrayscale(bool enable)
{
    displayGrayscaleMode = enable;
    lv_obj_invalidate(lv_screen_active()); 
}

// Display initialization
DisplayInitResult display_init()
{
    bus = new Arduino_SWSPI(DISP_DC, DISP_CS, DISP_SCK, DISP_MOSI, DISP_MISO);
    if (!bus) return DisplayInitResult::BUS_FAIL;

    rgbpanel = new Arduino_ESP32RGBPanel(
        DISP_DE, DISP_VSYNC, DISP_HSYNC, DISP_PCLK,
        DISP_R1, DISP_R2, DISP_R3, DISP_R4, DISP_R5,
        DISP_G0, DISP_G1, DISP_G2, DISP_G3, DISP_G4, DISP_G5,
        DISP_B1, DISP_B2, DISP_B3, DISP_B4, DISP_B5,
        DISP_HSYNC_POL, DISP_HFRONT_PORCH, DISP_HPULSE_WIDTH, DISP_HBACK_PORCH,
        DISP_VSYNC_POL, DISP_VFRONT_PORCH, DISP_VPULSE_WIDTH, DISP_VBACK_PORCH,
        DISP_PCLK_ACT_NEG, DISP_PREFER_SPEED, DISP_BOUNCE);
    if (!rgbpanel) return DisplayInitResult::RGB_FAIL;

    gfx = new Arduino_RGB_Display(
        DISP_WIDTH, DISP_HEIGHT, rgbpanel, 0, true,
        bus, DISP_RST, DISP_INIT_SEQ, sizeof(DISP_INIT_SEQ));
    if (!gfx) return DisplayInitResult::GFX_ALLOC_FAIL;

    if (!gfx->begin()) return DisplayInitResult::GFX_BEGIN_FAIL;
    gfx->setRotation(DISP_ROTATION);

    if (!Wire.begin(TOUCH_SDA, TOUCH_SCL)) return DisplayInitResult::I2C_FAIL;

    uint8_t touch_addr = 0;

#if defined(TOUCH_TYPE_GT911)
    touch.dev = new TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, DISP_WIDTH, DISP_HEIGHT);
    if (!touch.dev) return DisplayInitResult::TOUCH_FAIL;

    touch.dev->begin();
    touch.dev->setRotation(TOUCH_ROTATION);

    uint8_t gt911_possible_addresses[] = {0x5D, 0x14};
    for (uint8_t addr : gt911_possible_addresses)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            touch_addr = addr;
            break;
        }
    }

    if (touch_addr == 0x14) 
    {
        touch.dev->begin(0x14);
        touch.dev->setRotation(TOUCH_ROTATION);
    }
#endif

#if defined(TOUCH_TYPE_FT6X36)
    touch.dev = new FT6X36(TOUCH_INT, TOUCH_RST);
    if (!touch.dev) return DisplayInitResult::TOUCH_FAIL;

    touch.dev->begin();
    detected_touch_addr = 0x38;
#endif

#if defined(TOUCH_TYPE_CST816)
    touch.dev = new cst816t(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST);
    if (!touch.dev) return DisplayInitResult::TOUCH_FAIL;

    touch.dev->begin();
    detected_touch_addr = 0x15;
#endif

    if (touch.dev != nullptr)
    {
        if (touch_addr == 0) return DisplayInitResult::TOUCH_FAIL;

        Wire.beginTransmission(touch_addr);
        if (Wire.endTransmission() != 0)
        {
            return DisplayInitResult::TOUCH_FAIL;
        }
    }

    lv_init();

    size_t buf_size = (DISP_WIDTH * LV_BUF_ROWS) * sizeof(lv_color_t);

#if defined(DISPLAY_TYPE_GUITION)
    lv_buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!lv_buf1) return DisplayInitResult::LV_BUF_FAIL;
    lv_buf2 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!lv_buf2)
    {
        heap_caps_free(lv_buf1);
        return DisplayInitResult::LV_BUF_FAIL; 
    }
    lv_display = lv_display_create(DISP_WIDTH, DISP_HEIGHT);
    if (!lv_display)
    {
        heap_caps_free(lv_buf1);
        heap_caps_free(lv_buf2);
        return DisplayInitResult::LV_DISPLAY_FAIL;
    }

    lv_display_set_flush_cb(lv_display, my_disp_flush);
    lv_display_set_buffers(lv_display, lv_buf1, lv_buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#else    
    lv_buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!lv_buf1) return DisplayInitResult::LV_BUF_FAIL;

    lv_display = lv_display_create(DISP_WIDTH, DISP_HEIGHT);
    if (!lv_display)
    {
        heap_caps_free(lv_buf1);
        return DisplayInitResult::LV_DISPLAY_FAIL;
    }

    lv_display_set_flush_cb(lv_display, my_disp_flush);
    lv_display_set_buffers(lv_display, lv_buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    lv_display_set_default(lv_display);

    lv_indev_t *indev = lv_indev_create();
    if (!indev) return DisplayInitResult::LV_INDEV_FAIL;

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    lv_indev_set_display(indev, lv_display);

    const esp_timer_create_args_t tick_args =
    {
        .callback = [](void *)
        { lv_tick_inc(DISPLAYTASK_INTERVAL); },
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick"
    };
    if (esp_timer_create(&tick_args, &lvgl_tick_timer) != ESP_OK) return DisplayInitResult::TIMER_FAIL;

    esp_timer_start_periodic(lvgl_tick_timer, DISPLAYTASK_INTERVAL * 1000);

#if defined(DISPLAY_AHT20)
    dt_aht20 = aht20.begin();
#endif

    initBacklight();

#if defined(DISPLAY_SPLASH_SCREEN)
    SplashScreen();
    lv_timer_handler();
    lv_refr_now(NULL);
    delay(DISPLAYTASK_INTERVAL * 4);
    setBacklight(BACKLIGHT_DEFAULT);
#endif

    return DisplayInitResult::OK;
}

// Display callbacks
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint32_t total_pixels = w * h;

    if (displayGrayscaleMode)
    {
        uint16_t *pixel_buffer = (uint16_t *)px_map;

        for (uint32_t i = 0; i < total_pixels; i++)
        {
            uint16_t color = pixel_buffer[i];

            uint8_t r = (color >> 11) & 0x1F;
            uint8_t g = (color >> 5) & 0x3F;
            uint8_t b = color & 0x1F;

            uint8_t gray_r = (r * 255) / 31;
            uint8_t gray_g = (g * 255) / 63;
            uint8_t gray_b = (b * 255) / 31;
            
            uint8_t gray = (uint8_t)((gray_r * 77 + gray_g * 150 + gray_b * 29) >> 8);

            uint16_t gray_r_5 = (gray * 31) / 255;
            uint16_t gray_g_6 = (gray * 63) / 255;
            uint16_t gray_b_5 = (gray * 31) / 255;

            pixel_buffer[i] = (gray_r_5 << 11) | (gray_g_6 << 5) | gray_b_5;
        }
    }

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (touch.blocked)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (touch.wait_release)
    {
        if (!touch.pressed)
        {
            touch.wait_release = false;
        }
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->point.x = touch.x;
    data->point.y = touch.y;
    data->state = touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#if defined(DISPLAY_SPLASH_SCREEN)
// Splash Screen
void SplashScreen()
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_set_style_bg_color( scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa( scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, &ot_icon_boot);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -40);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_scrollbar_mode(img, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "OTGateway");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_align_to(label, img, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    lv_obj_update_layout(scr);
}
#endif
