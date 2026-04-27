#include <LeanTask.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <Wire.h>
#if defined(USE_AHT20)
  #include <Adafruit_AHTX0.h>
#endif  

LV_FONT_DECLARE(mdi_24);

// HW pins
#define DISP_CS        1
#define DISP_SCK       12
#define DISP_SDA       11
#define DISP_RST       43
#define DISP_DE        45
#define DISP_HSYNC     5
#define DISP_VSYNC     4
#define DISP_PCLK      21
#define DISP_BACKLIGHT 46

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
#define DISP_WIDTH  480
#define DISP_HEIGHT 480

// Touch GT911
#define TOUCH_SDA   17
#define TOUCH_SCL   18
#define TOUCH_INT   10
#define TOUCH_RST   -1
#define TOUCH_GT911_ROTATION ROTATION_INVERTED

const char L_DISPLAY[]  PROGMEM = "DISPLAY";

// Externed vars/settings
extern Variables vars;
extern Settings settings;

// Enum for display initialization results
enum class DisplayInitResult : uint8_t {
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

const char* displayInitResultToString(DisplayInitResult r)
{
    switch (r) {
        case DisplayInitResult::OK:               return "OK";
        case DisplayInitResult::BUS_FAIL:         return "BUS_FAIL";
        case DisplayInitResult::RGB_FAIL:         return "RGB_FAIL";
        case DisplayInitResult::GFX_ALLOC_FAIL:   return "GFX_ALLOC_FAIL";
        case DisplayInitResult::GFX_BEGIN_FAIL:   return "GFX_BEGIN_FAIL";
        case DisplayInitResult::TOUCH_FAIL:       return "TOUCH_FAIL";
        case DisplayInitResult::LV_BUF_FAIL:      return "LV_BUF_FAIL";
        case DisplayInitResult::LV_DISPLAY_FAIL:  return "LV_DISPLAY_FAIL";
        case DisplayInitResult::LV_INDEV_FAIL:    return "LV_INDEV_FAIL";
        case DisplayInitResult::TIMER_FAIL:       return "TIMER_FAIL";
        default:                                  return "UNKNOWN";
    }
}

// AHT20 sensor
#if defined(USE_AHT20)
  Adafruit_AHTX0            aht;
#endif

// Global display objects
Arduino_DataBus          *init_bus      = nullptr;
Arduino_ESP32RGBPanel    *rgbpanel      = nullptr;
Arduino_RGB_Display      *gfx           = nullptr;
TAMC_GT911               *gt911         = nullptr;
lv_display_t             *lv_display    = nullptr;
lv_color_t               *lv_buf        = nullptr;
  
// Touch state
int16_t touch_x = 0;
int16_t touch_y = 0;
bool    touch_pressed = false;

// Display state
bool     display_on = true;
int      display_timeout = 30;
uint32_t display_last_touch = 0;

// LGVL Tick timer
esp_timer_handle_t lvgl_tick_timer = nullptr;

// UI objects
struct {
    // Heating
    lv_obj_t *heat_title = nullptr;
    lv_obj_t *heat_setpoint = nullptr;
    lv_obj_t *heat_current = nullptr;
    lv_obj_t *heat_btn_down = nullptr;
    lv_obj_t *heat_btn_up = nullptr;
    lv_obj_t *heat_sw_enable = nullptr;
    lv_obj_t *heat_sw_turbo = nullptr;
    lv_obj_t *heat_lbl_enable = nullptr;
    lv_obj_t *heat_lbl_turbo = nullptr;
    lv_obj_t *heat_flame_icon = nullptr;
    lv_obj_t *heat_icon_wifi = nullptr;
    // DHW
    lv_obj_t *dhw_title = nullptr;
    lv_obj_t *dhw_setpoint = nullptr;
    lv_obj_t *dhw_current = nullptr;
    lv_obj_t *dhw_btn_down = nullptr;
    lv_obj_t *dhw_btn_up = nullptr;
    lv_obj_t *dhw_sw_enable = nullptr;
    lv_obj_t *dhw_lbl_enable = nullptr;
    lv_obj_t *dhw_time = nullptr;
} ui;

// Forward declarations
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
void createDashboardUI();
void updateDashboardUI();

#if defined(USE_AHT20)
// AHT20 Sensor update
void update_aht20() {
    sensors_event_t humidity, temp;
    if (aht.getEvent(&humidity, &temp)) {
        aht20Sensor.temperature = temp.temperature;
        aht20Sensor.humidity = humidity.relative_humidity;
    }
}  
#endif

// Event callbacks
static void heat_down_cb(lv_event_t *e) {
    settings.heating.target = fmax(settings.heating.minTemp, settings.heating.target - 0.5f);
    Log.sinfoln(FPSTR(L_DISPLAY), F("Heating target: %.1f°C"), settings.heating.target);
}

static void heat_up_cb(lv_event_t *e) {
    settings.heating.target = fmin(settings.heating.maxTemp, settings.heating.target + 0.5f);
    Log.sinfoln(FPSTR(L_DISPLAY), F("Heating target: %.1f°C"), settings.heating.target);
}

static void heat_enable_cb(lv_event_t *e) {
    settings.heating.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    Log.sinfoln(FPSTR(L_DISPLAY), F("Heating: %s"), settings.heating.enabled ? "enabled" : "disabled");
}

static void heat_turbo_cb(lv_event_t *e) {
    settings.heating.turbo = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    Log.sinfoln(FPSTR(L_DISPLAY), F("Heating turbo: %s"), settings.heating.turbo ? "enabled" : "disabled");
}

static void dhw_down_cb(lv_event_t *e) {
    settings.dhw.target = fmax(settings.dhw.minTemp, settings.dhw.target - 0.5f);
    Log.sinfoln(FPSTR(L_DISPLAY), F("DHW target: %.1f°C"), settings.dhw.target);
}

static void dhw_up_cb(lv_event_t *e) {
    settings.dhw.target = fmin(settings.dhw.maxTemp, settings.dhw.target + 0.5f);
    Log.sinfoln(FPSTR(L_DISPLAY), F("DHW target: %.1f°C"), settings.dhw.target);
}

static void dhw_enable_cb(lv_event_t *e) {
    settings.dhw.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    Log.sinfoln(FPSTR(L_DISPLAY), F("DHW: %s"), settings.dhw.enabled ? "enabled" : "disabled");
}

// Display functions
void displayOn() {
  display_on = true;
  pinMode(DISP_BACKLIGHT, OUTPUT);
  digitalWrite(DISP_BACKLIGHT, HIGH);
  display_last_touch = millis();
}
    
void displayOff() {
  display_on = false;
  pinMode(DISP_BACKLIGHT, OUTPUT);
  digitalWrite(DISP_BACKLIGHT, LOW);
}

// Display initialization
DisplayInitResult display_init()
{
  init_bus = new Arduino_SWSPI(-1, DISP_CS, DISP_SCK, DISP_SDA, -1);
  if (!init_bus) return DisplayInitResult::BUS_FAIL;

  rgbpanel = new Arduino_ESP32RGBPanel(
    DISP_DE /* DE */, DISP_VSYNC /* VSYNC */, DISP_HSYNC /* HSYNC */, DISP_PCLK /* PCLK */,
    DISP_R1, DISP_R2, DISP_R3, DISP_R4, DISP_R5,            // R
    DISP_G0, DISP_G1, DISP_G2, DISP_G3, DISP_G4, DISP_G5,   // G
    DISP_B1, DISP_B2, DISP_B3, DISP_B4, DISP_B5,            // B
    1 /* hsync_polarity */, 40, 48, 120,
    1 /* vsync_polarity */, 20 /* v_front */, 10 /* v_pulse */, 40 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 10000000L /* 10MHz */,
    false /* bounce_buffer */);
  if (!rgbpanel) return DisplayInitResult::RGB_FAIL;

  gfx = new Arduino_RGB_Display(
    DISP_WIDTH, DISP_HEIGHT, rgbpanel, 0, true,
    init_bus, DISP_RST, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));
    if (!gfx) return DisplayInitResult::GFX_ALLOC_FAIL;

  if (!gfx->begin()) return DisplayInitResult::GFX_BEGIN_FAIL;

  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  gt911 = new TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, DISP_WIDTH, DISP_HEIGHT);
  if (!gt911) return DisplayInitResult::TOUCH_FAIL;

  gt911->begin();
  gt911->setRotation(TOUCH_GT911_ROTATION);

  lv_init();

  uint32_t buf_pixels = DISP_WIDTH * DISP_HEIGHT;
  size_t buf_size = buf_pixels * sizeof(lv_color_t);

  lv_buf = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!lv_buf) {
    Log.swarningln(FPSTR(L_DISPLAY), F("PSRAM buffer failed, trying INTERNAL"));
    lv_buf = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL);
  }
  if (!lv_buf) return DisplayInitResult::LV_BUF_FAIL;

  lv_display = lv_display_create(DISP_WIDTH, DISP_HEIGHT);
  if (!lv_display) return DisplayInitResult::LV_DISPLAY_FAIL;

  lv_display_set_flush_cb(lv_display, my_disp_flush);
  lv_display_set_buffers(lv_display, lv_buf, nullptr, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(lv_display);

  lv_indev_t *indev = lv_indev_create();
  if (!indev) return DisplayInitResult::LV_INDEV_FAIL;

  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  lv_indev_set_display(indev, lv_display);

  const esp_timer_create_args_t tick_args = {
    .callback = [](void*) { lv_tick_inc(5); },
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lvgl_tick"
  };
  if (esp_timer_create(&tick_args, &lvgl_tick_timer) != ESP_OK) return DisplayInitResult::TIMER_FAIL;

  esp_timer_start_periodic(lvgl_tick_timer, 5000);

#if defined(USE_AHT20)
  if (!aht.begin()) {
    Log.swarningln(FPSTR(L_DISPLAY), F("AHT20 sensor not found!"));
  }
#endif

  createDashboardUI();

  displayOn();

  return DisplayInitResult::OK;
}

// Display callbacks
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  data->point.x = touch_x;
  data->point.y = touch_y;
  data->state = touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// UI Creation
void createDashboardUI() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF5F5F5), 0);

    // ===== HEATING SECTION (TOP) =====
    lv_obj_t *heat_cont = lv_obj_create(scr);
    lv_obj_set_size(heat_cont, 460, 220);
    lv_obj_set_pos(heat_cont, 10, 20);
    lv_obj_set_style_bg_color(heat_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(heat_cont, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(heat_cont, 2, 0);
    lv_obj_set_style_pad_all(heat_cont, 0, 0);
    lv_obj_set_scrollbar_mode(heat_cont, LV_SCROLLBAR_MODE_OFF);

    // Title background bar
    lv_obj_t *heat_title_bg = lv_obj_create(heat_cont);
    lv_obj_set_size(heat_title_bg, 460, 40);
    lv_obj_set_pos(heat_title_bg, 0, 0);
    lv_obj_set_style_bg_color(heat_title_bg, lv_color_hex(0xF0F8FF), 0);
    lv_obj_set_style_border_width(heat_title_bg, 0, 0);
    lv_obj_set_scrollbar_mode(heat_title_bg, LV_SCROLLBAR_MODE_OFF);

    ui.heat_title = lv_label_create(heat_title_bg);
    lv_label_set_text(ui.heat_title, "Heating");
    lv_obj_set_style_text_font(ui.heat_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.heat_title, lv_color_hex(0x0066CC), 0);
    lv_obj_align(ui.heat_title, LV_ALIGN_CENTER, 0, 0);

    // Flame icon
    ui.heat_flame_icon = lv_label_create(heat_title_bg);
    lv_obj_set_style_text_font(ui.heat_flame_icon, &mdi_24, 0);
    lv_label_set_text(ui.heat_flame_icon, "\xF3\xB0\x88\xB8");
    lv_obj_set_style_text_color(ui.heat_flame_icon, lv_color_hex(0x0066CC), 0);
    lv_obj_align(ui.heat_flame_icon, LV_ALIGN_LEFT_MID, 0, -1);
    lv_obj_add_flag(ui.heat_flame_icon, LV_OBJ_FLAG_HIDDEN);    

    // WIFI icon
    ui.heat_icon_wifi = lv_label_create(heat_title_bg);
    lv_label_set_text(ui.heat_icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui.heat_icon_wifi, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.heat_icon_wifi, lv_color_hex(0x0066CC), 0);
    lv_obj_add_flag(ui.heat_icon_wifi, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(ui.heat_icon_wifi, LV_ALIGN_RIGHT_MID, -5, -1);

    // Down button
    ui.heat_btn_down = lv_button_create(heat_cont);
    lv_obj_set_size(ui.heat_btn_down, 50, 50);
    lv_obj_set_pos(ui.heat_btn_down, 15, 60);
    lv_obj_set_style_bg_color(ui.heat_btn_down, lv_color_hex(0xE8F4FF), 0);
    lv_obj_set_style_border_color(ui.heat_btn_down, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_border_width(ui.heat_btn_down, 2, 0);
    lv_obj_add_event_cb(ui.heat_btn_down, heat_down_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl = lv_label_create(ui.heat_btn_down);
    lv_label_set_text(lbl, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0066CC), 0);
    lv_obj_center(lbl);

    // Setpoint temp (CENTER ONLY)
    ui.heat_setpoint = lv_label_create(heat_cont);
    lv_label_set_text(ui.heat_setpoint, "21 °C");
    lv_obj_set_style_text_font(ui.heat_setpoint, &lv_font_montserrat_24, 0);
    lv_obj_align(ui.heat_setpoint, LV_ALIGN_CENTER, 0, -25);

    // Current temp (below setpoint)
    ui.heat_current = lv_label_create(heat_cont);
    lv_label_set_text(ui.heat_current, "Current: 22 °C");
    lv_obj_set_style_text_font(ui.heat_current, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.heat_current, lv_color_hex(0x666666), 0);
    lv_obj_align(ui.heat_current, LV_ALIGN_CENTER, 0, 10);

    // Up button
    ui.heat_btn_up = lv_button_create(heat_cont);
    lv_obj_set_size(ui.heat_btn_up, 50, 50);
    lv_obj_set_pos(ui.heat_btn_up, 395, 60);
    lv_obj_set_style_bg_color(ui.heat_btn_up, lv_color_hex(0xE8F4FF), 0);
    lv_obj_set_style_border_color(ui.heat_btn_up, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_border_width(ui.heat_btn_up, 2, 0);
    lv_obj_add_event_cb(ui.heat_btn_up, heat_up_cb, LV_EVENT_CLICKED, nullptr);
    
    lbl = lv_label_create(ui.heat_btn_up);
    lv_label_set_text(lbl, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0066CC), 0);
    lv_obj_center(lbl);

    // Enable switch
    ui.heat_sw_enable = lv_switch_create(heat_cont);
    lv_obj_set_size(ui.heat_sw_enable, 40, 24);
    lv_obj_set_pos(ui.heat_sw_enable, 20, 165);
    lv_obj_add_event_cb(ui.heat_sw_enable, heat_enable_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    if (settings.heating.enabled) lv_obj_add_state(ui.heat_sw_enable, LV_STATE_CHECKED);

    ui.heat_lbl_enable = lv_label_create(heat_cont);
    lv_label_set_text(ui.heat_lbl_enable, "Enable");
    lv_obj_set_style_text_font(ui.heat_lbl_enable, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ui.heat_lbl_enable, 65, 170);

    // Turbo switch
    ui.heat_sw_turbo = lv_switch_create(heat_cont);
    lv_obj_set_size(ui.heat_sw_turbo, 40, 24);
    lv_obj_set_pos(ui.heat_sw_turbo, 260, 165);
    lv_obj_add_event_cb(ui.heat_sw_turbo, heat_turbo_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    if (settings.heating.turbo) lv_obj_add_state(ui.heat_sw_turbo, LV_STATE_CHECKED);

    ui.heat_lbl_turbo = lv_label_create(heat_cont);
    lv_label_set_text(ui.heat_lbl_turbo, "Turbo mode");
    lv_obj_set_style_text_font(ui.heat_lbl_turbo, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ui.heat_lbl_turbo, 305, 170);

    // ===== DHW SECTION (BOTTOM) =====
    lv_obj_t *dhw_cont = lv_obj_create(scr);
    lv_obj_set_size(dhw_cont, 460, 220);
    lv_obj_set_pos(dhw_cont, 10, 250);
    lv_obj_set_style_bg_color(dhw_cont, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(dhw_cont, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(dhw_cont, 2, 0);
    lv_obj_set_style_pad_all(dhw_cont, 0, 0);
    lv_obj_set_scrollbar_mode(dhw_cont, LV_SCROLLBAR_MODE_OFF);

    // Title background bar
    lv_obj_t *dhw_title_bg = lv_obj_create(dhw_cont);
    lv_obj_set_size(dhw_title_bg, 460, 40);
    lv_obj_set_pos(dhw_title_bg, 0, 0);
    lv_obj_set_style_bg_color(dhw_title_bg, lv_color_hex(0xF0F8FF), 0);
    lv_obj_set_style_border_width(dhw_title_bg, 0, 0);
    lv_obj_set_scrollbar_mode(dhw_title_bg, LV_SCROLLBAR_MODE_OFF);

    ui.dhw_title = lv_label_create(dhw_title_bg);
    lv_label_set_text(ui.dhw_title, "DHW");
    lv_obj_set_style_text_font(ui.dhw_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.dhw_title, lv_color_hex(0x0066CC), 0);
    lv_obj_align(ui.dhw_title, LV_ALIGN_CENTER, 0, 0);

    // Down button
    ui.dhw_btn_down = lv_button_create(dhw_cont);
    lv_obj_set_size(ui.dhw_btn_down, 50, 50);
    lv_obj_set_pos(ui.dhw_btn_down, 15, 60);
    lv_obj_set_style_bg_color(ui.dhw_btn_down, lv_color_hex(0xE8F4FF), 0);
    lv_obj_set_style_border_color(ui.dhw_btn_down, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_border_width(ui.dhw_btn_down, 2, 0);
    lv_obj_add_event_cb(ui.dhw_btn_down, dhw_down_cb, LV_EVENT_CLICKED, nullptr);
    
    lbl = lv_label_create(ui.dhw_btn_down);
    lv_label_set_text(lbl, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0066CC), 0);
    lv_obj_center(lbl);

    // Setpoint temp (CENTER ONLY)
    ui.dhw_setpoint = lv_label_create(dhw_cont);
    lv_label_set_text(ui.dhw_setpoint, "30 °C");
    lv_obj_set_style_text_font(ui.dhw_setpoint, &lv_font_montserrat_24, 0);
    lv_obj_align(ui.dhw_setpoint, LV_ALIGN_CENTER, 0, -25);

    // Current temp (below setpoint)
    ui.dhw_current = lv_label_create(dhw_cont);
    lv_label_set_text(ui.dhw_current, "Current: 40.8 °C");
    lv_obj_set_style_text_font(ui.dhw_current, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.dhw_current, lv_color_hex(0x666666), 0);
    lv_obj_align(ui.dhw_current, LV_ALIGN_CENTER, 0, 10);

    // Up button
    ui.dhw_btn_up = lv_button_create(dhw_cont);
    lv_obj_set_size(ui.dhw_btn_up, 50, 50);
    lv_obj_set_pos(ui.dhw_btn_up, 395, 60);
    lv_obj_set_style_bg_color(ui.dhw_btn_up, lv_color_hex(0xE8F4FF), 0);
    lv_obj_set_style_border_color(ui.dhw_btn_up, lv_color_hex(0x0066CC), 0);
    lv_obj_set_style_border_width(ui.dhw_btn_up, 2, 0);
    lv_obj_add_event_cb(ui.dhw_btn_up, dhw_up_cb, LV_EVENT_CLICKED, nullptr);
    
    lbl = lv_label_create(ui.dhw_btn_up);
    lv_label_set_text(lbl, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x0066CC), 0);
    lv_obj_center(lbl);

    // Enable switch
    ui.dhw_sw_enable = lv_switch_create(dhw_cont);
    lv_obj_set_size(ui.dhw_sw_enable, 40, 24);
    lv_obj_set_pos(ui.dhw_sw_enable, 20, 165);
    lv_obj_add_event_cb(ui.dhw_sw_enable, dhw_enable_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    if (settings.dhw.enabled) lv_obj_add_state(ui.dhw_sw_enable, LV_STATE_CHECKED);

    ui.dhw_lbl_enable = lv_label_create(dhw_cont);
    lv_label_set_text(ui.dhw_lbl_enable, "Enable");
    lv_obj_set_style_text_font(ui.dhw_lbl_enable, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(ui.dhw_lbl_enable, 65, 170);

    // Time label
    ui.dhw_time = lv_label_create(dhw_cont);
    lv_label_set_text(ui.dhw_time, "--:--");
    lv_obj_set_style_text_font(ui.dhw_time, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui.dhw_time, lv_color_hex(0x333333), 0);
    lv_obj_align(ui.dhw_time, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
}

// UI Update
void updateDashboardUI() {
    // Update heating section
    if (ui.heat_setpoint) {
        int t10 = (int)(settings.heating.target * 10.0f);
        lv_label_set_text_fmt(ui.heat_setpoint, "%d.%d °C", t10 / 10, abs(t10 % 10));
    }
    
    if (ui.heat_current) {
        int t10 = (int)(vars.slave.heating.currentTemp * 10.0f);
        lv_label_set_text_fmt(ui.heat_current, "Current: %d.%d °C", t10 / 10, abs(t10 % 10));
    }

    if (ui.heat_sw_enable) {
        if (settings.heating.enabled) lv_obj_add_state(ui.heat_sw_enable, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui.heat_sw_enable, LV_STATE_CHECKED);
    }

    if (ui.heat_sw_turbo) {
        if (settings.heating.turbo) lv_obj_add_state(ui.heat_sw_turbo, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui.heat_sw_turbo, LV_STATE_CHECKED);
    }

    if (ui.heat_title) {
        if (vars.slave.heating.active) {
            lv_obj_set_style_text_color(ui.heat_title, lv_color_hex(0xC62828), 0);
        } else {
            lv_obj_set_style_text_color(ui.heat_title, lv_color_hex(0x0066CC), 0);
        }
    }

    if (vars.slave.flame) {
        lv_obj_clear_flag(ui.heat_flame_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui.heat_flame_icon, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui.heat_icon_wifi) {
        if (vars.network.connected)
            lv_obj_clear_flag(ui.heat_icon_wifi, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(ui.heat_icon_wifi, LV_OBJ_FLAG_HIDDEN);
    }

    // Update DHW section
    if (ui.dhw_setpoint) {
        int t10 = (int)(settings.dhw.target * 10.0f);
        lv_label_set_text_fmt(ui.dhw_setpoint, "%d.%d °C", t10 / 10, abs(t10 % 10));
    }
    
    if (ui.dhw_current) {
        int t10 = (int)(vars.slave.dhw.currentTemp * 10.0f);
        lv_label_set_text_fmt(ui.dhw_current, "Current: %d.%d °C", t10 / 10, abs(t10 % 10));
    }

    if (ui.dhw_sw_enable) {
        if (settings.dhw.enabled) lv_obj_add_state(ui.dhw_sw_enable, LV_STATE_CHECKED);
        else lv_obj_clear_state(ui.dhw_sw_enable, LV_STATE_CHECKED);
    }

    if (ui.dhw_title) {
        if (vars.slave.dhw.active) {
            lv_obj_set_style_text_color(ui.dhw_title, lv_color_hex(0xC62828), 0);
        } else {
            lv_obj_set_style_text_color(ui.dhw_title, lv_color_hex(0x0066CC), 0);
        }
    }

    if (ui.dhw_time) {
        time_t now = time(nullptr);
        if (now > 100000) {
            struct tm timeinfo;
            if (localtime_r(&now, &timeinfo)) {
                char buf[6];
                snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                lv_label_set_text(ui.dhw_time, buf);
            }    
        }
    }
}

// Display loop
void display_loop() {
    static bool last_touch = false;

    gt911->read();
    bool current_touch = gt911->isTouched;

    if (current_touch) {
        touch_x = gt911->points[0].x;
        touch_y = gt911->points[0].y;
    }
    touch_pressed = current_touch;
    if (current_touch && !last_touch) {
        if (!display_on) displayOn();
        display_last_touch = millis();
    }
    last_touch = current_touch;

#if defined(USE_AHT20)
    if (aht20Sensor.read) {
        if ((millis() - aht20Sensor.last_read) > aht20Sensor.period) {
            aht20Sensor.last_read = millis();
            update_aht20();
        }
    }
#endif

    if (display_on &&
        (millis() - display_last_touch > display_timeout * 1000)) {
        displayOff();
    }

    if (!display_on) return;

    updateDashboardUI();
    lv_timer_handler();
}

// Display Task definition
class DisplayTask : public LeanTask {
public:
    DisplayTask(bool _enabled = true, uint32_t _interval = 5)
        : LeanTask(_enabled, _interval) {}

private:
    void setup() override {}
    void loop() override {
        if (vars.states.restarting || vars.states.upgrading) {
          return;
        }

        display_loop();
    }
};