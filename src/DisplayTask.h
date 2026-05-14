#include <LeanTask.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Wire.h>

#if defined(USE_AHT20)
#include <Adafruit_AHTX0.h>
#endif

#include "display/dt_local.h"

LV_FONT_DECLARE(mdi_24);
LV_FONT_DECLARE(lv_font_chinese_20);
LV_FONT_DECLARE(lv_font_chinese_24);
LV_FONT_DECLARE(lv_font_montserrat_20_ext);
LV_FONT_DECLARE(lv_font_montserrat_24_ext);

#if defined(USE_SPLASH_SCREEN)
LV_IMG_DECLARE(ot_icon_boot);
#endif

// HW pins
#define DISP_CS 1
#define DISP_SCK 12
#define DISP_SDA 11
#define DISP_RST 43
#define DISP_DE 45
#define DISP_HSYNC 5
#define DISP_VSYNC 4
#define DISP_PCLK 21
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

// Display resolution
#define DISP_WIDTH 480
#define DISP_HEIGHT 480

// Touch GT911
#define TOUCH_SDA 17
#define TOUCH_SCL 18
#define TOUCH_INT 10
#define TOUCH_RST GFX_NOT_DEFINED
#define TOUCH_GT911_ROTATION ROTATION_INVERTED

// DisplayTask Interval
#define DISPLAYTASK_INTERVAL 16

// Display strings
const char L_DISPLAY[] PROGMEM = "DISPLAY";

// Externed vars/settings
extern Variables vars;
extern Settings settings;

// Enum for display initialization results
enum class DisplayInitResult : uint8_t
{
    OK = 0,
    BUS_FAIL,
    RGB_FAIL,
    GFX_ALLOC_FAIL,
    GFX_BEGIN_FAIL,
    TOUCH_FAIL,
    LV_BUF_FAIL,
    LV_DISPLAY_FAIL,
    LV_INDEV_FAIL,
    TIMER_FAIL
};

const char *displayInitResultToString(DisplayInitResult r)
{
    switch (r)
    {
    case DisplayInitResult::OK:
        return "OK";
    case DisplayInitResult::BUS_FAIL:
        return "BUS_FAIL";
    case DisplayInitResult::RGB_FAIL:
        return "RGB_FAIL";
    case DisplayInitResult::GFX_ALLOC_FAIL:
        return "GFX_ALLOC_FAIL";
    case DisplayInitResult::GFX_BEGIN_FAIL:
        return "GFX_BEGIN_FAIL";
    case DisplayInitResult::TOUCH_FAIL:
        return "TOUCH_FAIL";
    case DisplayInitResult::LV_BUF_FAIL:
        return "LV_BUF_FAIL";
    case DisplayInitResult::LV_DISPLAY_FAIL:
        return "LV_DISPLAY_FAIL";
    case DisplayInitResult::LV_INDEV_FAIL:
        return "LV_INDEV_FAIL";
    case DisplayInitResult::TIMER_FAIL:
        return "TIMER_FAIL";
    default:
        return "UNKNOWN";
    }
}

// Temperature unit string
static inline const char *temperatureUnit()
{
    return (settings.system.unitSystem ==
            UnitSystem::METRIC)
               ? "°C"
               : "°F";
}

// AHT20 sensor
#if defined(USE_AHT20)
Adafruit_AHTX0 aht20;
#endif

// Global display objects
Arduino_DataBus *bus = nullptr;
Arduino_ESP32RGBPanel *rgbpanel = nullptr;
Arduino_RGB_Display *gfx = nullptr;
TAMC_GT911 *gt911 = nullptr;
lv_display_t *lv_display = nullptr;
static lv_color_t *lv_buf1 = nullptr;
static lv_color_t *lv_buf2 = nullptr;

// LGVL Tick timer
esp_timer_handle_t lvgl_tick_timer = nullptr;

// UI objects
struct
{
    // Root/pages
    lv_obj_t *root = nullptr;
    lv_obj_t *page_heat = nullptr;
    lv_obj_t *page_dhw = nullptr;
    lv_obj_t *page_display = nullptr;

    // ===== COMMON =====
    lv_obj_t *heat_icon_flame = nullptr;
    lv_obj_t *heat_icon_wifi = nullptr;
    lv_obj_t *heat_icon_opentherm = nullptr;
    lv_obj_t *heat_icon_mqtt = nullptr;
    lv_obj_t *heat_time = nullptr;

    lv_obj_t *dhw_icon_flame = nullptr;
    lv_obj_t *dhw_icon_wifi = nullptr;
    lv_obj_t *dhw_icon_opentherm = nullptr;
    lv_obj_t *dhw_icon_mqtt = nullptr;
    lv_obj_t *dhw_time = nullptr;

    lv_obj_t *display_icon_wifi = nullptr;
    lv_obj_t *display_icon_opentherm = nullptr;
    lv_obj_t *display_icon_mqtt = nullptr;
    lv_obj_t *display_time = nullptr;

    // ===== HEATING PAGE =====
    lv_obj_t *heat_arc = nullptr;
    lv_obj_t *heat_setpoint = nullptr;
    lv_obj_t *heat_current = nullptr;
    lv_obj_t *heat_action = nullptr;
    lv_obj_t *heat_btn_on = nullptr;
    lv_obj_t *heat_btn_next = nullptr;
    lv_obj_t *heat_btn_prev = nullptr;
    lv_obj_t *heat_btn_turbo = nullptr;

    // ===== DHW PAGE =====
    lv_obj_t *dhw_arc = nullptr;
    lv_obj_t *dhw_setpoint = nullptr;
    lv_obj_t *dhw_current = nullptr;
    lv_obj_t *dhw_action = nullptr;
    lv_obj_t *dhw_btn_on = nullptr;
    lv_obj_t *dhw_btn_next = nullptr;
    lv_obj_t *dhw_btn_prev = nullptr;

    // ===== DISPLAY PAGE =====
    lv_obj_t *display_arc = nullptr;
    lv_obj_t *display_setpoint = nullptr;
    lv_obj_t *display_current = nullptr;
    lv_obj_t *display_action = nullptr;
    lv_obj_t *display_btn_on = nullptr;
    lv_obj_t *display_btn_next = nullptr;
    lv_obj_t *display_btn_prev = nullptr;
    lv_obj_t *display_slider = nullptr;
    lv_obj_t *display_ip = nullptr;
    
    // ===== BUTTON LABELS =====
    lv_obj_t *heat_btn_turbo_label = nullptr;
    lv_obj_t *heat_btn_on_label = nullptr;
    lv_obj_t *dhw_btn_on_label = nullptr;
    lv_obj_t *display_btn_on_label = nullptr;
} ui;

// Last values for display update optimization
struct
{
    int heating_target10 = -1;
    int heating_currentTemp10 = -1;
    int heating_minTemp10 = -1;
    int heating_maxTemp10 = -1;
    bool heating_enabled = false;
    bool heating_active = false;
    bool heating_turbo = false;
    int dhw_target = -1;
    int dhw_currentTemp = -1;
    int dhw_minTemp = -1;
    int dhw_maxTemp = -1;
    bool dhw_enabled = false;
    bool dhw_active = false;
    int display_brightness = -1;
    int display_timeout = -1;
    bool flame = false;
    bool wifi = false;
    bool opentherm = false;
    bool mqtt = false;
    int last_minute = -1;
    Language language = Language::EN;
} LastVals;

struct
{
    int16_t x = 0;
    int16_t y = 0;
    bool pressed = false;
    bool last_state = false;
    bool blocked = true;
    bool wait_release = false;
} touch;

struct
{
    bool on = true;
    bool to_save = false;
    uint32_t last_touch = 0;
    uint32_t last_save = 0;
} display;

// Forward declarations
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
void createSplashScreen();
void createDashboardUI();
void updateDashboardUI();

#if defined(USE_AHT20)
// AHT20 sensor update
void update_aht20()
{
    sensors_event_t humidity, temp;
    if (aht20.getEvent(&humidity, &temp))
    {
        SensorAHT20.temperature = temp.temperature;
        SensorAHT20.humidity = humidity.relative_humidity;
    }
}
#endif

// Page switching callbacks
static void show_heat_page()
{
    lv_obj_clear_flag(ui.page_heat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.page_dhw, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.page_display, LV_OBJ_FLAG_HIDDEN);
}

static void show_dhw_page()
{
    lv_obj_clear_flag(ui.page_dhw, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.page_heat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.page_display, LV_OBJ_FLAG_HIDDEN);
}

static void show_display_page()
{
    lv_obj_clear_flag(ui.page_display, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.page_heat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.page_dhw, LV_OBJ_FLAG_HIDDEN);
}

// Event callbacks
static void heat_enable_cb(lv_event_t *e)
{
    settings.heating.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void heat_turbo_cb(lv_event_t *e)
{
    settings.heating.turbo = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void dhw_enable_cb(lv_event_t *e)
{
    settings.dhw.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void display_enable_cb(lv_event_t *e)
{
    settings.display.language = static_cast<Language>((static_cast<uint8_t>(settings.display.language) + 1) % 7);
}

static void display_timeout_cb(lv_event_t *e)
{
    settings.display.timeout_ms = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e)) * 1000;
    display.to_save = true;
}

// Backlight functions
void initBacklight()
{
    ledcAttach(BACKLIGHT_PIN, BACKLIGHT_FREQ, BACKLIGHT_RES);
    ledcWrite(BACKLIGHT_PIN, 0);
}

void setBacklight(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    float normalized = brightness / 100.0f;
    float gamma = powf(normalized, BACKLIGHT_GAMMA);
    float range = BACKLIGHT_MAX - BACKLIGHT_MIN;
    uint8_t output = (uint8_t)(BACKLIGHT_MIN + gamma * range + 0.5f);
    ledcWrite(BACKLIGHT_PIN, output);
}

// Display functions
void displayOn()
{
    display.on = true;
    setBacklight(settings.display.brightness);
}

void displayOff()
{
    display.on = false;
    ledcWrite(BACKLIGHT_PIN, 0); //Backlight off
}

// Display initialization
DisplayInitResult display_init()
{
    bus = new Arduino_SWSPI(GFX_NOT_DEFINED, DISP_CS, DISP_SCK, DISP_SDA, GFX_NOT_DEFINED);
    if (!bus) return DisplayInitResult::BUS_FAIL;

    rgbpanel = new Arduino_ESP32RGBPanel(
        DISP_DE /* DE */, DISP_VSYNC /* VSYNC */, DISP_HSYNC /* HSYNC */, DISP_PCLK /* PCLK */,
        DISP_R1, DISP_R2, DISP_R3, DISP_R4, DISP_R5,          // R
        DISP_G0, DISP_G1, DISP_G2, DISP_G3, DISP_G4, DISP_G5, // G
        DISP_B1, DISP_B2, DISP_B3, DISP_B4, DISP_B5,          // B
        1 /* hsync_polarity */, 40, 48, 120,
        1 /* vsync_polarity */, 20 /* v_front */, 10 /* v_pulse */, 40 /* vsync_back_porch */,
        1 /* pclk_active_neg */, 10000000L /* 10MHz */,
        false /* bounce_buffer */);
    if (!rgbpanel) return DisplayInitResult::RGB_FAIL;

    gfx = new Arduino_RGB_Display(
        DISP_WIDTH, DISP_HEIGHT, rgbpanel, 0, true,
        bus, DISP_RST, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));
    if (!gfx) return DisplayInitResult::GFX_ALLOC_FAIL;

    if (!gfx->begin()) return DisplayInitResult::GFX_BEGIN_FAIL;

    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    gt911 = new TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, DISP_WIDTH, DISP_HEIGHT);
    if (!gt911) return DisplayInitResult::TOUCH_FAIL;

    gt911->begin();
    gt911->setRotation(TOUCH_GT911_ROTATION);

    lv_init();

    size_t buf_size = (DISP_WIDTH * 40) * sizeof(lv_color_t);

    lv_buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
/*  Internal buffer only
    lv_buf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
*/    
    if (!lv_buf1) return DisplayInitResult::LV_BUF_FAIL;

    lv_display = lv_display_create(DISP_WIDTH, DISP_HEIGHT);
    if (!lv_display) return DisplayInitResult::LV_DISPLAY_FAIL;

    lv_display_set_flush_cb(lv_display, my_disp_flush);
    lv_display_set_buffers(lv_display, lv_buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_default(lv_display);

    lv_indev_t *indev = lv_indev_create();
    if (!indev) return DisplayInitResult::LV_INDEV_FAIL;

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    lv_indev_set_display(indev, lv_display);

    const esp_timer_create_args_t tick_args = {
        .callback = [](void *)
        { lv_tick_inc(DISPLAYTASK_INTERVAL); },
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick"};
    if (esp_timer_create(&tick_args, &lvgl_tick_timer) != ESP_OK) return DisplayInitResult::TIMER_FAIL;

    esp_timer_start_periodic(lvgl_tick_timer, DISPLAYTASK_INTERVAL * 1000);

#if defined(USE_AHT20)
    if (!aht20.begin())
    {
        Log.swarningln(FPSTR(L_DISPLAY), F("AHT20 sensor not found!"));
    }
    else
    {
        SensorAHT20.found = true;
    }
#endif

    initBacklight();

#if defined(USE_SPLASH_SCREEN)
    createSplashScreen();
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

#if defined(USE_SPLASH_SCREEN)
// Splash Screen Creation
void createSplashScreen()
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

// UI Creation
void createDashboardUI()
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_t *lbl;

    static lv_style_t style_btn_off;
    static lv_style_t style_btn_on_heat;
    static lv_style_t style_btn_on_dhw;
    static lv_style_t style_btn_on_display;

    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // ---- BUTTONS DEFAULT ----
    lv_style_init(&style_btn_off);
    lv_style_set_bg_opa(&style_btn_off, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_btn_off, 2);
    lv_style_set_border_color(&style_btn_off, lv_color_hex(0x808080));
    lv_style_set_text_color(&style_btn_off, lv_color_white());
    lv_style_set_radius(&style_btn_off, 10);

    lv_style_init(&style_btn_on_heat);
    lv_style_set_bg_opa(&style_btn_on_heat, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_on_heat, lv_color_hex(0xFF6F22));
    lv_style_set_border_width(&style_btn_on_heat, 2);
    lv_style_set_border_color(&style_btn_on_heat, lv_color_white());
    lv_style_set_text_color(&style_btn_on_heat, lv_color_white());
    lv_style_set_radius(&style_btn_on_heat, 10);

    lv_style_init(&style_btn_on_dhw);
    lv_style_set_bg_opa(&style_btn_on_dhw, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_on_dhw, lv_color_hex(0x2095F6));
    lv_style_set_border_width(&style_btn_on_dhw, 2);
    lv_style_set_border_color(&style_btn_on_dhw, lv_color_white());
    lv_style_set_text_color(&style_btn_on_dhw, lv_color_white());
    lv_style_set_radius(&style_btn_on_dhw, 10);

    lv_style_init(&style_btn_on_display);
    lv_style_set_bg_opa(&style_btn_on_display, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_on_display, lv_color_hex(0x1B5E20));
    lv_style_set_border_width(&style_btn_on_display, 2);
    lv_style_set_border_color(&style_btn_on_display, lv_color_white());
    lv_style_set_text_color(&style_btn_on_display, lv_color_white());
    lv_style_set_radius(&style_btn_on_display, 10);

    // =====================================================
    // ROOT
    // =====================================================
    ui.root = lv_obj_create(scr);
    lv_obj_set_size(ui.root, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_pos(ui.root, 0, 0);

    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_set_style_border_width(ui.root, 0, 0);
    lv_obj_set_style_radius(ui.root, 0, 0);
    lv_obj_set_style_bg_color(ui.root, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);

    // =====================================================
    // HEATING PAGE
    // =====================================================
    ui.page_heat = lv_obj_create(ui.root);

    lv_obj_set_size(ui.page_heat, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_pos(ui.page_heat, 0, 0);

    lv_obj_clear_flag(ui.page_heat, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.page_heat, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_set_style_border_width(ui.page_heat, 0, 0);
    lv_obj_set_style_radius(ui.page_heat, 0, 0);
    lv_obj_set_style_bg_color(ui.page_heat, lv_color_black(), 0);

    // ---- Flame icon ----
    ui.heat_icon_flame = lv_label_create(ui.page_heat);
    lv_obj_set_style_text_font(ui.heat_icon_flame, &mdi_24, 0);
    lv_label_set_text(ui.heat_icon_flame, "\xF3\xB0\x88\xB8");
    lv_obj_set_style_text_color(ui.heat_icon_flame, lv_color_hex(0xFF6F22), 0);
    lv_obj_align(ui.heat_icon_flame, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_flag(ui.heat_icon_flame, LV_OBJ_FLAG_HIDDEN);

    // ---- WIFI icon ----
    ui.heat_icon_wifi = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui.heat_icon_wifi, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.heat_icon_wifi, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.heat_icon_wifi, LV_ALIGN_TOP_RIGHT, -9, 4);

    // ---- OT icon ----
    ui.heat_icon_opentherm = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_icon_opentherm, "O");
    lv_obj_set_style_text_font(ui.heat_icon_opentherm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.heat_icon_opentherm, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.heat_icon_opentherm, LV_ALIGN_TOP_RIGHT, -37, 4);

    // ---- MQTT icon ----
    ui.heat_icon_mqtt = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_icon_mqtt, "M");
    lv_obj_set_style_text_font(ui.heat_icon_mqtt, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.heat_icon_mqtt, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.heat_icon_mqtt, LV_ALIGN_TOP_RIGHT, -57, 4);

    // ---- Time ----
    ui.heat_time = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_time, "--:--");
    lv_obj_set_style_text_font(ui.heat_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.heat_time, lv_color_white(), 0);
    lv_obj_align(ui.heat_time, LV_ALIGN_TOP_LEFT, 4, 4);

    // ---- ARC ----
    ui.heat_arc = lv_arc_create(ui.page_heat);

    lv_obj_set_size(ui.heat_arc, 420, 420);
    lv_obj_align(ui.heat_arc, LV_ALIGN_CENTER, 0, -20);

    LastVals.heating_minTemp10 = settings.display.heating_minTemp10;
    LastVals.heating_maxTemp10 = settings.display.heating_maxTemp10;

    lv_arc_set_range(ui.heat_arc, LastVals.heating_minTemp10, LastVals.heating_maxTemp10);
    lv_arc_set_bg_angles(ui.heat_arc, 135, 45);
    lv_arc_set_value(ui.heat_arc, settings.heating.target * 10.0f);

    lv_obj_set_style_arc_width(ui.heat_arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.heat_arc, lv_color_hex(0x292929), LV_PART_MAIN);

    lv_obj_set_style_arc_width(ui.heat_arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.heat_arc, lv_color_hex(0xFF6F22), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(ui.heat_arc, lv_color_white(), LV_PART_KNOB);

    // ---- LABELS ----
    ui.heat_action = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_action, "");
    lv_obj_set_style_text_font(ui.heat_action, &lv_font_montserrat_24_ext, 0);
    lv_obj_set_style_text_color(ui.heat_action, lv_color_hex(0xFF6F22), 0);
    lv_obj_align(ui.heat_action, LV_ALIGN_CENTER, 0, -120);

    ui.heat_setpoint = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_setpoint, "--.-°");
    lv_obj_set_style_text_font(ui.heat_setpoint, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui.heat_setpoint, lv_color_white(), 0);
    lv_obj_align(ui.heat_setpoint, LV_ALIGN_CENTER, 0, -20);

    ui.heat_current = lv_label_create(ui.page_heat);
    lv_label_set_text(ui.heat_current, "--.-°");
    lv_obj_set_style_text_font(ui.heat_current, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ui.heat_current, lv_color_hex(0xFF6F22), 0);
    lv_obj_align(ui.heat_current, LV_ALIGN_CENTER, 0, 60);

    // ---- BUTTONS ----
    // TURBO
    ui.heat_btn_turbo = lv_button_create(ui.page_heat);

    lv_obj_set_size(ui.heat_btn_turbo, 100, 50);
    lv_obj_align(ui.heat_btn_turbo, LV_ALIGN_BOTTOM_LEFT, 120, -5);

    lv_obj_add_style(ui.heat_btn_turbo, &style_btn_off, LV_PART_MAIN);
    lv_obj_add_style(ui.heat_btn_turbo, &style_btn_on_heat, LV_STATE_CHECKED);

    lv_obj_add_flag(ui.heat_btn_turbo, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_add_event_cb(ui.heat_btn_turbo, heat_turbo_cb, LV_EVENT_CLICKED, nullptr);

    ui.heat_btn_turbo_label = lv_label_create(ui.heat_btn_turbo);
    lv_label_set_text(ui.heat_btn_turbo_label, tr(settings.display.language,DisplayText::TURBO));
    lv_obj_set_style_text_font(ui.heat_btn_turbo_label, &lv_font_montserrat_20_ext, 0);
    lv_obj_center(ui.heat_btn_turbo_label);

    // HEAT
    ui.heat_btn_on = lv_button_create(ui.page_heat);

    lv_obj_set_size(ui.heat_btn_on, 100, 50);
    lv_obj_align(ui.heat_btn_on, LV_ALIGN_BOTTOM_RIGHT, -120, -5);

    lv_obj_add_style(ui.heat_btn_on, &style_btn_off, LV_PART_MAIN);
    lv_obj_add_style(ui.heat_btn_on, &style_btn_on_heat, LV_STATE_CHECKED);

    lv_obj_add_flag(ui.heat_btn_on, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_add_event_cb(ui.heat_btn_on, heat_enable_cb, LV_EVENT_CLICKED, nullptr);

    ui.heat_btn_on_label = lv_label_create(ui.heat_btn_on);
    lv_label_set_text(ui.heat_btn_on_label, tr(settings.display.language,DisplayText::HEAT));
    lv_obj_set_style_text_font(ui.heat_btn_on_label, &lv_font_montserrat_20_ext, 0);
    lv_obj_center(ui.heat_btn_on_label);

    // PREV, NEXT PAGE
    ui.heat_btn_prev = lv_button_create(ui.page_heat);

    lv_obj_set_size(ui.heat_btn_prev, 54, 54);
    lv_obj_align(ui.heat_btn_prev, LV_ALIGN_BOTTOM_LEFT, 1, -5);

    lv_obj_set_style_bg_opa(ui.heat_btn_prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.heat_btn_prev, 2, 0);
    lv_obj_set_style_border_color(ui.heat_btn_prev, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(ui.heat_btn_prev, 27, 0);

    lv_obj_add_event_cb(
        ui.heat_btn_prev,
        [](lv_event_t *e)
        {
            show_display_page();
        },
        LV_EVENT_CLICKED,
        NULL);

    lbl = lv_label_create(ui.heat_btn_prev);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    ui.heat_btn_next = lv_button_create(ui.page_heat);

    lv_obj_set_size(ui.heat_btn_next, 54, 54);
    lv_obj_align(ui.heat_btn_next, LV_ALIGN_BOTTOM_RIGHT, -1, -5);

    lv_obj_set_style_bg_opa(ui.heat_btn_next, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.heat_btn_next, 2, 0);
    lv_obj_set_style_border_color(ui.heat_btn_next, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(ui.heat_btn_next, 27, 0);

    lv_obj_add_event_cb(
        ui.heat_btn_next,
        [](lv_event_t *e)
        {
            show_dhw_page();
        },
        LV_EVENT_CLICKED,
        NULL);

    lbl = lv_label_create(ui.heat_btn_next);
    lv_label_set_text(lbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    // =====================================================
    // DHW PAGE
    // =====================================================
    ui.page_dhw = lv_obj_create(ui.root);

    lv_obj_set_size(ui.page_dhw, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_pos(ui.page_dhw, 0, 0);

    lv_obj_clear_flag(ui.page_dhw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.page_dhw, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_set_style_border_width(ui.page_dhw, 0, 0);
    lv_obj_set_style_radius(ui.page_dhw, 0, 0);
    lv_obj_set_style_bg_color(ui.page_dhw, lv_color_black(), 0);

    // Hidden by default
    lv_obj_add_flag(ui.page_dhw, LV_OBJ_FLAG_HIDDEN);

    // ---- Flame icon ----
    ui.dhw_icon_flame = lv_label_create(ui.page_dhw);
    lv_obj_set_style_text_font(ui.dhw_icon_flame, &mdi_24, 0);
    lv_label_set_text(ui.dhw_icon_flame, "\xF3\xB0\x88\xB8");
    lv_obj_set_style_text_color(ui.dhw_icon_flame, lv_color_hex(0x2095F6), 0);
    lv_obj_align(ui.dhw_icon_flame, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_flag(ui.dhw_icon_flame, LV_OBJ_FLAG_HIDDEN);

    // ---- WIFI icon ----
    ui.dhw_icon_wifi = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui.dhw_icon_wifi, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.dhw_icon_wifi, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.dhw_icon_wifi, LV_ALIGN_TOP_RIGHT, -9, 4);

    // ---- OT icon ----
    ui.dhw_icon_opentherm = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_icon_opentherm, "O");
    lv_obj_set_style_text_font(ui.dhw_icon_opentherm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.dhw_icon_opentherm, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.dhw_icon_opentherm, LV_ALIGN_TOP_RIGHT, -37, 4);

    // ---- MQTT icon ----
    ui.dhw_icon_mqtt = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_icon_mqtt, "M");
    lv_obj_set_style_text_font(ui.dhw_icon_mqtt, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.dhw_icon_mqtt, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.dhw_icon_mqtt, LV_ALIGN_TOP_RIGHT, -57, 4);

    // ---- Time ----
    ui.dhw_time = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_time, "--:--");
    lv_obj_set_style_text_font(ui.dhw_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.dhw_time, lv_color_white(), 0);
    lv_obj_align(ui.dhw_time, LV_ALIGN_TOP_LEFT, 4, 4);

    // ---- ARC ----
    ui.dhw_arc = lv_arc_create(ui.page_dhw);

    lv_obj_set_size(ui.dhw_arc, 420, 420);
    lv_obj_align(ui.dhw_arc, LV_ALIGN_CENTER, 0, -20);

    LastVals.dhw_minTemp = (int)(settings.dhw.minTemp);
    LastVals.dhw_maxTemp = (int)(settings.dhw.maxTemp);

    lv_arc_set_range(ui.dhw_arc, LastVals.dhw_minTemp, LastVals.dhw_maxTemp);
    lv_arc_set_bg_angles(ui.dhw_arc, 135, 45);
    lv_arc_set_value(ui.dhw_arc, settings.dhw.target);

    lv_obj_set_style_arc_width(ui.dhw_arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.dhw_arc, lv_color_hex(0x292929), LV_PART_MAIN);

    lv_obj_set_style_arc_width(ui.dhw_arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.dhw_arc, lv_color_hex(0x2095F6), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(ui.dhw_arc, lv_color_white(), LV_PART_KNOB);

    // ---- LABELS ----
    ui.dhw_action = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_action, "");
    lv_obj_set_style_text_font(ui.dhw_action, &lv_font_montserrat_24_ext, 0);
    lv_obj_set_style_text_color(ui.dhw_action, lv_color_hex(0x2095F6), 0);
    lv_obj_align(ui.dhw_action, LV_ALIGN_CENTER, 0, -120);

    ui.dhw_setpoint = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_setpoint, "--.-°");
    lv_obj_set_style_text_font(ui.dhw_setpoint, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui.dhw_setpoint, lv_color_white(), 0);
    lv_obj_align(ui.dhw_setpoint, LV_ALIGN_CENTER, 0, -20);

    ui.dhw_current = lv_label_create(ui.page_dhw);
    lv_label_set_text(ui.dhw_current, "--.-°");
    lv_obj_set_style_text_font(ui.dhw_current, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ui.dhw_current, lv_color_hex(0x2095F6), 0);
    lv_obj_align(ui.dhw_current, LV_ALIGN_CENTER, 0, 60);

    // ---- BUTTONS ----
    // DHW
    ui.dhw_btn_on = lv_button_create(ui.page_dhw);

    lv_obj_set_size(ui.dhw_btn_on, 100, 50);
    lv_obj_align(ui.dhw_btn_on, LV_ALIGN_BOTTOM_RIGHT, -120, -5);

    lv_obj_add_style(ui.dhw_btn_on, &style_btn_off, LV_PART_MAIN);
    lv_obj_add_style(ui.dhw_btn_on, &style_btn_on_dhw, LV_STATE_CHECKED);

    lv_obj_add_flag(ui.dhw_btn_on, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_add_event_cb(ui.dhw_btn_on, dhw_enable_cb, LV_EVENT_CLICKED, nullptr);

    ui.dhw_btn_on_label = lv_label_create(ui.dhw_btn_on);
    lv_label_set_text(ui.dhw_btn_on_label, tr(settings.display.language,DisplayText::DHW));
    lv_obj_set_style_text_font(ui.dhw_btn_on_label, &lv_font_montserrat_20_ext, 0);
    lv_obj_center(ui.dhw_btn_on_label);

    // PREV, NEXT PAGE
    ui.dhw_btn_prev = lv_button_create(ui.page_dhw);

    lv_obj_set_size(ui.dhw_btn_prev, 54, 54);
    lv_obj_align(ui.dhw_btn_prev, LV_ALIGN_BOTTOM_LEFT, 1, -5);

    lv_obj_set_style_bg_opa(ui.dhw_btn_prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.dhw_btn_prev, 2, 0);
    lv_obj_set_style_border_color(ui.dhw_btn_prev, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(ui.dhw_btn_prev, 27, 0);

    lv_obj_add_event_cb(
        ui.dhw_btn_prev,
        [](lv_event_t *e)
        {
            show_heat_page();
        },
        LV_EVENT_CLICKED,
        NULL);

    lbl = lv_label_create(ui.dhw_btn_prev);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    ui.dhw_btn_next = lv_button_create(ui.page_dhw);

    lv_obj_set_size(ui.dhw_btn_next, 54, 54);
    lv_obj_align(ui.dhw_btn_next, LV_ALIGN_BOTTOM_RIGHT, -1, -5);

    lv_obj_set_style_bg_opa(ui.dhw_btn_next, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.dhw_btn_next, 2, 0);
    lv_obj_set_style_border_color(ui.dhw_btn_next, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(ui.dhw_btn_next, 27, 0);

    lv_obj_add_event_cb(
        ui.dhw_btn_next,
        [](lv_event_t *e)
        {
            show_display_page();
        },
        LV_EVENT_CLICKED,
        NULL);

    lbl = lv_label_create(ui.dhw_btn_next);
    lv_label_set_text(lbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    // =====================================================
    // DISPLAY PAGE
    // =====================================================
    ui.page_display = lv_obj_create(ui.root);

    lv_obj_set_size(ui.page_display, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_pos(ui.page_display, 0, 0);

    lv_obj_clear_flag(ui.page_display, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.page_display, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_set_style_border_width(ui.page_display, 0, 0);
    lv_obj_set_style_radius(ui.page_display, 0, 0);
    lv_obj_set_style_bg_color(ui.page_display, lv_color_black(), 0);

    // Hidden by default
    lv_obj_add_flag(ui.page_display, LV_OBJ_FLAG_HIDDEN);

    // ---- WIFI icon ----
    ui.display_icon_wifi = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui.display_icon_wifi, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.display_icon_wifi, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.display_icon_wifi, LV_ALIGN_TOP_RIGHT, -9, 4);

    // ---- OT icon ----
    ui.display_icon_opentherm = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_icon_opentherm, "O");
    lv_obj_set_style_text_font(ui.display_icon_opentherm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.display_icon_opentherm, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.display_icon_opentherm, LV_ALIGN_TOP_RIGHT, -37, 4);

    // ---- MQTT icon ----
    ui.display_icon_mqtt = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_icon_mqtt, "M");
    lv_obj_set_style_text_font(ui.display_icon_mqtt, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.display_icon_mqtt, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.display_icon_mqtt, LV_ALIGN_TOP_RIGHT, -57, 4);

    // ---- Time ----
    ui.display_time = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_time, "--:--");
    lv_obj_set_style_text_font(ui.display_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.display_time, lv_color_white(), 0);
    lv_obj_align(ui.display_time, LV_ALIGN_TOP_LEFT, 4, 4);

    // ---- ARC ----
    ui.display_arc = lv_arc_create(ui.page_display);

    lv_obj_set_size(ui.display_arc, 420, 420);
    lv_obj_align(ui.display_arc, LV_ALIGN_CENTER, 0, -20);

    lv_arc_set_range(ui.display_arc, 0, 100);
    lv_arc_set_bg_angles(ui.display_arc, 135, 45);
    lv_arc_set_value(ui.display_arc, settings.dhw.target);

    lv_obj_set_style_arc_width(ui.display_arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.display_arc, lv_color_hex(0x292929), LV_PART_MAIN);

    lv_obj_set_style_arc_width(ui.display_arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.display_arc, lv_color_hex(0x1B5E20), LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(ui.display_arc, lv_color_white(), LV_PART_KNOB);

    // ---- LABELS ----
    ui.display_action = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_action, tr(settings.display.language,DisplayText::BRIGHTNESS));
    lv_obj_set_style_text_font(ui.display_action, &lv_font_montserrat_24_ext, 0);
    lv_obj_set_style_text_color(ui.display_action, lv_color_hex(0x1B5E20), 0);
    lv_obj_align(ui.display_action, LV_ALIGN_CENTER, 0, -120);

    ui.display_setpoint = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_setpoint, "0%");
    lv_obj_set_style_text_font(ui.display_setpoint, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui.display_setpoint, lv_color_white(), 0);
    lv_obj_align(ui.display_setpoint, LV_ALIGN_CENTER, 0, -20);

    ui.display_current = lv_label_create(ui.page_display);
    lv_label_set_text_fmt(ui.display_current, "%s: %ds", tr(settings.display.language,DisplayText::TIMEOUT), 0);
    lv_obj_set_style_text_font(ui.display_current, &lv_font_montserrat_24_ext, 0);
    lv_obj_set_style_text_color(ui.display_current, lv_color_hex(0x1B5E20), 0);
    lv_obj_align(ui.display_current, LV_ALIGN_CENTER, 0, 60);

    ui.display_ip = lv_label_create(ui.page_display);
    lv_label_set_text(ui.display_ip, "");
    lv_obj_set_style_text_font(ui.display_ip, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.display_ip, lv_color_hex(0x808080), 0);
    lv_obj_align(ui.display_ip, LV_ALIGN_BOTTOM_LEFT, 80, -20); 

    // ---- BUTTONS ----
    // LANG
    ui.display_btn_on = lv_button_create(ui.page_display);

    lv_obj_set_size(ui.display_btn_on, 100, 50);
    lv_obj_align(ui.display_btn_on, LV_ALIGN_BOTTOM_RIGHT, -120, -5);

    lv_obj_add_style(ui.display_btn_on, &style_btn_on_display, LV_PART_MAIN);

    lv_obj_add_event_cb(ui.display_btn_on, display_enable_cb, LV_EVENT_CLICKED, nullptr);

    ui.display_btn_on_label = lv_label_create(ui.display_btn_on);
    lv_obj_set_style_text_font(ui.display_btn_on_label, &lv_font_montserrat_20, 0);
    lv_obj_center(ui.display_btn_on_label);

    // PREV, NEXT PAGE
    ui.display_btn_prev = lv_button_create(ui.page_display);

    lv_obj_set_size(ui.display_btn_prev, 54, 54);
    lv_obj_align(ui.display_btn_prev, LV_ALIGN_BOTTOM_LEFT, 1, -5);

    lv_obj_set_style_bg_opa(ui.display_btn_prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.display_btn_prev, 2, 0);
    lv_obj_set_style_border_color(ui.display_btn_prev, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(ui.display_btn_prev, 27, 0);

    lv_obj_add_event_cb(
        ui.display_btn_prev,
        [](lv_event_t *e)
        {
            show_dhw_page();
        },
        LV_EVENT_CLICKED,
        NULL);

    lbl = lv_label_create(ui.display_btn_prev);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    ui.display_btn_next = lv_button_create(ui.page_display);

    lv_obj_set_size(ui.display_btn_next, 54, 54);
    lv_obj_align(ui.display_btn_next, LV_ALIGN_BOTTOM_RIGHT, -1, -5);

    lv_obj_set_style_bg_opa(ui.display_btn_next, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.display_btn_next, 2, 0);
    lv_obj_set_style_border_color(ui.display_btn_next, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(ui.display_btn_next, 27, 0);

    lv_obj_add_event_cb(
        ui.display_btn_next,
        [](lv_event_t *e)
        {
            show_heat_page();
        },
        LV_EVENT_CLICKED,
        NULL);

    lbl = lv_label_create(ui.display_btn_next);
    lv_label_set_text(lbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    // TIMEOUT SLIDER
    ui.display_slider  = lv_slider_create(ui.page_display);

    lv_obj_set_size(ui.display_slider, 200, 12);
    lv_obj_align(ui.display_slider, LV_ALIGN_CENTER, 0, 128);

    lv_slider_set_range(ui.display_slider, 0, 120);

    lv_obj_set_style_bg_color(ui.display_slider, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.display_slider, lv_color_hex(0x1B5E20), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui.display_slider, lv_color_white(), LV_PART_KNOB);

    lv_obj_add_event_cb(ui.display_slider, display_timeout_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // =====================================================
    // ARC EVENTS
    // =====================================================
    lv_obj_add_event_cb(
        ui.heat_arc,
        [](lv_event_t *e)
        {
            lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

            int value = lv_arc_get_value(arc);

            float temp = value / 10.0f;

            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f%s", temp, temperatureUnit());

            lv_label_set_text(ui.heat_setpoint, buf);
        },
        LV_EVENT_VALUE_CHANGED,
        NULL);

    lv_obj_add_event_cb(
        ui.heat_arc,
        [](lv_event_t *e)
        {
            lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

            int value = lv_arc_get_value(arc);

            settings.heating.target = value / 10.0f;
            display.to_save = true;
        },
        LV_EVENT_RELEASED,
        NULL);

    lv_obj_add_event_cb(
        ui.dhw_arc,
        [](lv_event_t *e)
        {
            lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

            int value = lv_arc_get_value(arc);

            char buf[16];
            snprintf(buf, sizeof(buf), "%d%s", value, temperatureUnit());

            lv_label_set_text(ui.dhw_setpoint, buf);
        },
        LV_EVENT_VALUE_CHANGED,
        NULL);

    lv_obj_add_event_cb(
        ui.dhw_arc,
        [](lv_event_t *e)
        {
            lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

            int value = lv_arc_get_value(arc);

            settings.dhw.target = value;
            display.to_save = true;
        },
        LV_EVENT_RELEASED,
        NULL);

    lv_obj_add_event_cb(
        ui.display_arc,
        [](lv_event_t *e)
        {
            lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

            int value = lv_arc_get_value(arc);

            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", value);

            lv_label_set_text(ui.display_setpoint, buf);
        },
        LV_EVENT_VALUE_CHANGED,
        NULL);

    lv_obj_add_event_cb(
        ui.display_arc,
        [](lv_event_t *e)
        {
            lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);

            int value = lv_arc_get_value(arc);

            settings.display.brightness = value;
            setBacklight(settings.display.brightness);
            display.to_save = true;
        },
        LV_EVENT_RELEASED,
        NULL);

    // =====================================================
    // INITIAL VALUES
    // =====================================================
    lv_arc_set_value(ui.heat_arc, (int)(settings.heating.target * 10.0f));
    lv_arc_set_value(ui.dhw_arc, (int)(settings.dhw.target));
    lv_arc_set_value(ui.display_arc, settings.display.brightness);
    lv_slider_set_value(ui.display_slider, int(settings.display.timeout_ms / 1000), LV_ANIM_OFF);

    LastVals.heating_enabled = settings.heating.enabled;
    if (ui.heat_btn_on)
    {
        if (settings.heating.enabled)
        {
            lv_obj_add_state(ui.heat_btn_on, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui.heat_btn_on, LV_STATE_CHECKED);
        }
    }

    LastVals.heating_turbo = settings.heating.turbo;
    if (ui.heat_btn_turbo)
    {
        if (settings.heating.turbo)
        {
            lv_obj_add_state(ui.heat_btn_turbo, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui.heat_btn_turbo, LV_STATE_CHECKED);
        }
    }

    LastVals.dhw_enabled = settings.dhw.enabled;
    if (ui.dhw_btn_on)
    {
        if (settings.dhw.enabled)
        {
            lv_obj_add_state(ui.dhw_btn_on, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui.dhw_btn_on, LV_STATE_CHECKED);
        }
    }

    LastVals.heating_active = !vars.slave.heating.active;
    LastVals.dhw_active = !vars.slave.dhw.active;
    LastVals.flame = !vars.slave.flame;
    LastVals.wifi = !vars.network.connected;
    LastVals.mqtt = !vars.mqtt.connected;
    LastVals.opentherm = !vars.slave.connected;

    show_heat_page();
}

// UI Update language
static void updateLanguage()
{
    if (ui.heat_btn_on_label)
    {
        lv_label_set_text(ui.heat_btn_on_label, tr(settings.display.language,DisplayText::HEAT));
    }

    if (ui.heat_btn_turbo_label)
    {
        lv_label_set_text(ui.heat_btn_turbo_label, tr(settings.display.language,DisplayText::TURBO));
    }

    if (ui.dhw_btn_on_label)
    {
        lv_label_set_text(ui.dhw_btn_on_label, tr(settings.display.language,DisplayText::DHW));
    }

    if (ui.heat_action)
    {
        if (vars.slave.heating.active)
        {
            lv_label_set_text(ui.heat_action, tr(settings.display.language,DisplayText::HEAT_ACTION));
        }
        else
        {
            lv_label_set_text(ui.heat_action, "");
        }
    }

    if (ui.dhw_action)
    {
        if (vars.slave.dhw.active)
        {
            lv_label_set_text(ui.dhw_action, tr(settings.display.language,DisplayText::DHW_ACTION));
        }
        else
        {
            lv_label_set_text(ui.dhw_action, "");
        }
    }

    if (ui.display_action)
    {
        lv_label_set_text(ui.display_action, tr(settings.display.language,DisplayText::BRIGHTNESS));
    }

    if (ui.display_current)
    {
        int timeout_sec = settings.display.timeout_ms / 1000;
        lv_label_set_text_fmt(ui.display_current, "%s: %ds", tr(settings.display.language,DisplayText::TIMEOUT), timeout_sec);
    }
}

// UI Update
void updateDashboardUI()
{
    // Update heating section
    if (ui.heat_setpoint)
    {
        int t10 = (int)(settings.heating.target * 10.0f);
        if (LastVals.heating_target10 != t10)
        {
            LastVals.heating_target10 = t10;
            lv_label_set_text_fmt(ui.heat_setpoint, "%d.%d%s", t10 / 10, abs(t10 % 10), temperatureUnit());
            if (ui.heat_arc)
            {
                if (lv_arc_get_value(ui.heat_arc) != t10)
                {
                    lv_arc_set_value(ui.heat_arc, t10);
                }
            }
        }
    }

    if (ui.heat_current)
    {
        if ((LastVals.heating_minTemp10 != settings.display.heating_minTemp10) || (LastVals.heating_maxTemp10 != settings.display.heating_maxTemp10))
        {
            LastVals.heating_minTemp10 = settings.display.heating_minTemp10;
            LastVals.heating_maxTemp10 = settings.display.heating_maxTemp10;
            if (ui.heat_arc)
            {
                lv_arc_set_range(ui.heat_arc, LastVals.heating_minTemp10, LastVals.heating_maxTemp10);
            }
        }

        float currentTemp =
            vars.master.heating.indoorTempControl
                ? vars.master.heating.indoorTemp
                : vars.master.heating.currentTemp;

        int t10 = (int)(currentTemp * 10.0f);
        if (LastVals.heating_currentTemp10 != t10)
        {
            LastVals.heating_currentTemp10 = t10;
            lv_label_set_text_fmt(ui.heat_current, "%d.%d%s", t10 / 10, abs(t10 % 10), temperatureUnit());
        }
    }

    if (LastVals.heating_enabled != settings.heating.enabled)
    {
        LastVals.heating_enabled = settings.heating.enabled;
        if (ui.heat_btn_on)
        {
            if (settings.heating.enabled)
            {
                lv_obj_add_state(ui.heat_btn_on, LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_clear_state(ui.heat_btn_on, LV_STATE_CHECKED);
            }
        }
    }

    if (LastVals.heating_turbo != settings.heating.turbo)
    {
        LastVals.heating_turbo = settings.heating.turbo;
        if (ui.heat_btn_turbo)
        {
            if (settings.heating.turbo)
            {
                lv_obj_add_state(ui.heat_btn_turbo, LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_clear_state(ui.heat_btn_turbo, LV_STATE_CHECKED);
            }
        }
    }

    if (ui.heat_action)
    {
        if (LastVals.heating_active != vars.slave.heating.active)
        {
            LastVals.heating_active = vars.slave.heating.active;
            if (vars.slave.heating.active)
            {
                lv_label_set_text(ui.heat_action, tr(settings.display.language,DisplayText::HEAT_ACTION));
            }
            else
            {
                lv_label_set_text(ui.heat_action, "");
            }
        }
    }

    // Update DHW section
    if (ui.dhw_setpoint)
    {
        int t10 = (int)(settings.dhw.target);
        if (LastVals.dhw_target != t10)
        {
            LastVals.dhw_target = t10;
            lv_label_set_text_fmt(ui.dhw_setpoint, "%d%s", t10, temperatureUnit());
            if (ui.dhw_arc)
            {
                if (lv_arc_get_value(ui.dhw_arc) != t10)
                {
                    lv_arc_set_value(ui.dhw_arc, t10);
                }
            }
        }
    }

    if (ui.dhw_current)
    {
        if ((LastVals.dhw_minTemp != (int)(settings.dhw.minTemp)) || (LastVals.dhw_maxTemp != (int)(settings.dhw.maxTemp)))
        {
            LastVals.dhw_minTemp = (int)(settings.dhw.minTemp);
            LastVals.dhw_maxTemp = (int)(settings.dhw.maxTemp);
            if (ui.dhw_arc)
            {
                lv_arc_set_range(ui.dhw_arc, LastVals.dhw_minTemp, LastVals.dhw_maxTemp);
            }
        }

        int t10 = (int)(vars.master.dhw.currentTemp * 10.0f);
        if (LastVals.dhw_currentTemp != t10)
        {
            LastVals.dhw_currentTemp = t10;
            lv_label_set_text_fmt(ui.dhw_current, "%d.%d%s", t10 / 10, abs(t10 % 10), temperatureUnit());
        }
    }

    if (LastVals.dhw_enabled != settings.dhw.enabled)
    {
        LastVals.dhw_enabled = settings.dhw.enabled;
        if (ui.dhw_btn_on)
        {
            if (settings.dhw.enabled)
            {
                lv_obj_add_state(ui.dhw_btn_on, LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_clear_state(ui.dhw_btn_on, LV_STATE_CHECKED);
            }
        }
    }

    if (ui.dhw_action)
    {
        if (LastVals.dhw_active != vars.slave.dhw.active)
        {
            LastVals.dhw_active = vars.slave.dhw.active;
            if (vars.slave.dhw.active)
            {
                lv_label_set_text(ui.dhw_action, tr(settings.display.language,DisplayText::DHW_ACTION));
            }
            else
            {
                lv_label_set_text(ui.dhw_action, "");
            }
        }
    }

    // Update display section
    if (ui.display_setpoint)
    {
        if (LastVals.display_brightness != settings.display.brightness)
        {
            LastVals.display_brightness = settings.display.brightness;
            lv_label_set_text_fmt(ui.display_setpoint, "%d%%", LastVals.display_brightness);
            if (ui.display_arc)
            {
                lv_arc_set_value(ui.display_arc, LastVals.display_brightness);
            }
            setBacklight(settings.display.brightness);
        }
    }

    int timeout_sec = settings.display.timeout_ms / 1000;
    if (LastVals.display_timeout != timeout_sec)
    {
        LastVals.display_timeout = timeout_sec;
        if (ui.display_current)
        {
            lv_label_set_text_fmt(ui.display_current, "%s: %ds", tr(settings.display.language,DisplayText::TIMEOUT), timeout_sec);
        }
        if (ui.display_slider)
        {
            lv_slider_set_value(ui.display_slider, timeout_sec, LV_ANIM_OFF);
        }
        if (settings.display.timeout_ms == 0)
        {
            lv_obj_set_style_text_color(ui.display_current, lv_color_hex(0x808080), 0);
        }     
        else
        {
            lv_obj_set_style_text_color(ui.display_current, lv_color_hex(0x1B5E20), 0);
        }
    }

    if (ui.display_btn_on_label)
    {
        lv_label_set_text(ui.display_btn_on_label, languageShort(settings.display.language));
    }

    // Update common section
    if (LastVals.flame != vars.slave.flame)
    {
        LastVals.flame = vars.slave.flame;
        if (ui.heat_icon_flame)
        {
            if (LastVals.flame)
                lv_obj_clear_flag(ui.heat_icon_flame, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ui.heat_icon_flame, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui.dhw_icon_flame)
        {
            if (LastVals.flame)
                lv_obj_clear_flag(ui.dhw_icon_flame, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ui.dhw_icon_flame, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (LastVals.wifi != vars.network.connected)
    {
        LastVals.wifi = vars.network.connected;
        if (ui.heat_icon_wifi)
        {
            if (LastVals.wifi)
                lv_obj_set_style_text_color(ui.heat_icon_wifi, lv_color_white(), 0);
             else
                lv_obj_set_style_text_color(ui.heat_icon_wifi, lv_color_hex(0x808080), 0);
         }
        if (ui.dhw_icon_wifi)
        {
            if (LastVals.wifi)
                lv_obj_set_style_text_color(ui.dhw_icon_wifi, lv_color_white(), 0);
            else
                lv_obj_set_style_text_color(ui.dhw_icon_wifi, lv_color_hex(0x808080), 0);
        }
        if (ui.display_icon_wifi)
        {
            if (LastVals.wifi)
                lv_obj_set_style_text_color(ui.display_icon_wifi, lv_color_white(), 0);
            else
                lv_obj_set_style_text_color(ui.display_icon_wifi, lv_color_hex(0x808080), 0);
        }
        if (ui.display_ip)
        {
            if (LastVals.wifi)
                lv_label_set_text(ui.display_ip, WiFi.localIP().toString().c_str());
            else
                lv_label_set_text(ui.display_ip, "");
        }
    }

    if (LastVals.mqtt != vars.mqtt.connected)
    {
        LastVals.mqtt = vars.mqtt.connected;
        if (ui.heat_icon_mqtt)
        {
            if (LastVals.mqtt)
                lv_obj_set_style_text_color(ui.heat_icon_mqtt, lv_color_white(), 0);
             else
                lv_obj_set_style_text_color(ui.heat_icon_mqtt, lv_color_hex(0x808080), 0);
         }
        if (ui.dhw_icon_mqtt)
        {
            if (LastVals.mqtt)
                lv_obj_set_style_text_color(ui.dhw_icon_mqtt, lv_color_white(), 0);
            else
                lv_obj_set_style_text_color(ui.dhw_icon_mqtt, lv_color_hex(0x808080), 0);
        }
        if (ui.display_icon_mqtt)
        {
            if (LastVals.mqtt)
                lv_obj_set_style_text_color(ui.display_icon_mqtt, lv_color_white(), 0);
            else
                lv_obj_set_style_text_color(ui.display_icon_mqtt, lv_color_hex(0x808080), 0);
        }
    }

    if (LastVals.opentherm != vars.slave.connected)
    {
        LastVals.opentherm = vars.slave.connected;
        if (ui.heat_icon_opentherm)
        {
            if (LastVals.opentherm)
                lv_obj_set_style_text_color(ui.heat_icon_opentherm, lv_color_white(), 0);
             else
                lv_obj_set_style_text_color(ui.heat_icon_opentherm, lv_color_hex(0x808080), 0);
         }
        if (ui.dhw_icon_opentherm)
        {
            if (LastVals.opentherm)
                lv_obj_set_style_text_color(ui.dhw_icon_opentherm, lv_color_white(), 0);
            else
                lv_obj_set_style_text_color(ui.dhw_icon_opentherm, lv_color_hex(0x808080), 0);
        }
        if (ui.display_icon_opentherm)
        {
            if (LastVals.opentherm)
                lv_obj_set_style_text_color(ui.display_icon_opentherm, lv_color_white(), 0);
            else
                lv_obj_set_style_text_color(ui.display_icon_opentherm, lv_color_hex(0x808080), 0);
        }
    }

    time_t now = time(nullptr);
    if (now > 100000)
    {
        struct tm timeinfo;
        if (localtime_r(&now, &timeinfo))
        {
            if (LastVals.last_minute != timeinfo.tm_min)
            {
                LastVals.last_minute = timeinfo.tm_min;
                char buf[6];
                snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                if (ui.heat_time)
                    lv_label_set_text(ui.heat_time, buf);
                if (ui.dhw_time)
                    lv_label_set_text(ui.dhw_time, buf);
                if (ui.display_time)
                    lv_label_set_text(ui.display_time, buf);
            }
        }
    }
    
    // Update language
    if (LastVals.language != settings.display.language)
    {
        LastVals.language =
            settings.display.language;

        updateLanguage();
    }    
}

// Display Task definition
class DisplayTask : public LeanTask
{
public:
    DisplayTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

    void setActivePriority()
    {
        vTaskPrioritySet(xTaskGetCurrentTaskHandle(), ACTIVE_PRIORITY);
    }

    void setSleepPriority()
    {
        vTaskPrioritySet(xTaskGetCurrentTaskHandle(), SLEEP_PRIORITY);
    }

protected:

#if defined(ARDUINO_ARCH_ESP32)
    const char *getTaskName() override
    {
        return "DisplayTask";
    }

    /*
    BaseType_t getTaskCore() override 
    {
      return 1;
    }
    */

    int getTaskPriority() override
    {
        return ACTIVE_PRIORITY;
    }
#endif

private:
    const UBaseType_t ACTIVE_PRIORITY = 5;
    const UBaseType_t SLEEP_PRIORITY  = 1;

    void setup() override
    {

#if defined(USE_SPLASH_SCREEN)
        vTaskDelay(pdMS_TO_TICKS(settings.display.splash_time_ms)); 
#endif

        createDashboardUI();
        lv_refr_now(NULL);
        display.last_touch = millis();
        touch.blocked = false;
        touch.wait_release = true;        
        settings.display.enabled = true;
    }

    void loop() override
    {
        if (vars.states.restarting || vars.states.upgrading)
        {
            return;
        }

        gt911->read();
        bool current_touch = gt911->isTouched;

        if (current_touch)
        {
            touch.x = gt911->points[0].x;
            touch.y = gt911->points[0].y;
        }
        touch.pressed = current_touch;
        if (current_touch && !touch.last_state)
        {
            if (!display.on) 
            {
/* Switch page when sleep not in wakeup
                if (!lv_obj_has_flag(ui.page_display, LV_OBJ_FLAG_HIDDEN))
                {
                    show_heat_page();
                }
*/                    
                displayOn();
                setActivePriority();
                touch.wait_release = true;
            }
            display.last_touch = millis();
        }
        touch.last_state = current_touch;

#if defined(USE_AHT20)
        if (SensorAHT20.read)
        {
            if ((millis() - SensorAHT20.last_read) > SensorAHT20.period)
            {
                SensorAHT20.last_read = millis();
                update_aht20();
            }
        }
#endif

        if ((millis() - display.last_save) > settings.display.save_time_ms)
        {
                display.last_save = millis();
                if (display.to_save)
                {
                    fsSettings.update();
                    display.to_save = false;
                }
        }

        if (!(settings.display.timeout_ms == 0))
        {
            if (display.on && (millis() - display.last_touch > settings.display.timeout_ms))
            {
                displayOff();
                if (!lv_obj_has_flag(ui.page_display, LV_OBJ_FLAG_HIDDEN))  //Switch to heat page in sleep
                {
                    show_heat_page();
                }
                setSleepPriority();
            }
        }

        if (!display.on) return;

        lv_timer_handler();

        updateDashboardUI();
    }
};