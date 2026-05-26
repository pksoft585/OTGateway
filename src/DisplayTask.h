#include <LeanTask.h>

#include "display/dt_local.h"

#if defined(DISPLAY_TYPE_DIYLESS3)
#include "display/dt_st7701.h"
#endif

// Externed vars/settings
extern Variables vars;
extern Settings settings;

// LVGL Fonts
LV_FONT_DECLARE(mdi_24);
LV_FONT_DECLARE(lv_font_chinese_20);
LV_FONT_DECLARE(lv_font_chinese_24);
LV_FONT_DECLARE(lv_font_montserrat_20_ext);
LV_FONT_DECLARE(lv_font_montserrat_24_ext);

// LVGL Colors
static lv_color_t gray_color    = lv_color_hex(0x808080);
static lv_color_t arc_color     = lv_color_hex(0x292929);
static lv_color_t heat_color    = lv_color_hex(0xFF6F22);
static lv_color_t dhw_color     = lv_color_hex(0x2095F6);
static lv_color_t disp_color    = lv_color_hex(0x1B5E20);
static lv_color_t slider_color  = lv_color_hex(0x404040);

// Pages
enum class DisplayPage : uint8_t
{
    HEATING = 0,
    DHW,
    DISP,

    COUNT
};

// Forward declarations
static void heating_enable_cb(lv_event_t *e);
static void heating_turbo_cb(lv_event_t *e);
static void dhw_enable_cb(lv_event_t *e);
static void display_language_cb(lv_event_t *e);
static void display_timeout_cb(lv_event_t *e);
void updateUI();
void renderPage(DisplayPage page);

// Visibility flags
enum PageFlags : uint16_t
{
    PF_NONE            = 0,
    PF_ARC             = 1 << 0,
    PF_SETPOINT        = 1 << 1,
    PF_CURRENT         = 1 << 2,
    PF_ACTION          = 1 << 3,
    PF_BTN_MAIN1       = 1 << 4,
    PF_BTN_MAIN2       = 1 << 5,
    PF_BTN_NEXT        = 1 << 6,
    PF_BTN_PREV        = 1 << 7,
    PF_SLIDER          = 1 << 8,
    PF_INFO            = 1 << 9,
};

// Runtime values for page
struct PageValues
{
    int arcValue = 0;
    int arcMinValue = 0;
    int arcMaxValue = 1000;

    int currentValue = 0;

    bool enabled = false;
    bool turbo = false;
    bool active = false;
};

// Common values
struct CommonValues
{
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool openthermConnected = false;
    bool flame = false;
    uint32_t unixTime = 0;
    Language language = Language::EN;
};

// Last rendered values
struct LastValues
{
    // Last rendered common values
    CommonValues common;

    // Current runtime values
    CommonValues current;

    // Last rendered page values
    PageValues pages[
        static_cast<uint8_t>(DisplayPage::COUNT)
    ];
};

// Static page layout
struct PageLayout
{
    uint16_t flags = PF_NONE;

    lv_color_t color = gray_color;

    const char* flameIcon = "";

    // Static texts
    DisplayText actionText = DisplayText::NONE;

    DisplayText btnMain1Text = DisplayText::NONE;
    DisplayText btnMain2Text = DisplayText::NONE;

    // Navigation
    DisplayPage nextPage = DisplayPage::HEATING;
    DisplayPage prevPage = DisplayPage::HEATING;

    // Event callbacks
    lv_event_cb_t btnMain1Callback = nullptr;
    lv_event_cb_t btnMain2Callback = nullptr;
    lv_event_cb_t sliderCallback = nullptr;
};

// Static page layouts
static const PageLayout PAGE_LAYOUTS[] =
{
    // HEATING
    {
        .flags =
            PF_ARC |
            PF_SETPOINT |
            PF_CURRENT |
            PF_ACTION |
            PF_BTN_MAIN1 |
            PF_BTN_MAIN2 |
            PF_BTN_NEXT |
            PF_BTN_PREV,

        .color = heat_color,

        .flameIcon = "\xF3\xB0\x88\xB8",

        .actionText = DisplayText::HEAT_ACTION,

        .btnMain1Text = DisplayText::HEAT,
        .btnMain2Text = DisplayText::TURBO,

        .nextPage = DisplayPage::DHW,
        .prevPage = DisplayPage::DISP,

        .btnMain1Callback = heating_enable_cb,
        .btnMain2Callback = heating_turbo_cb
    },

    // DHW
    {
        .flags =
            PF_ARC |
            PF_SETPOINT |
            PF_CURRENT |
            PF_ACTION |
            PF_BTN_MAIN1 |
            PF_BTN_NEXT |
            PF_BTN_PREV,

        .color = dhw_color,

        .flameIcon = "\xF3\xB0\x88\xB8",

        .actionText = DisplayText::DHW_ACTION,

        .btnMain1Text = DisplayText::DHW,
        .btnMain2Text = DisplayText::NONE,

        .nextPage = DisplayPage::DISP,
        .prevPage = DisplayPage::HEATING,

        .btnMain1Callback = dhw_enable_cb
    },

    // DISPLAY
    {
        .flags =
            PF_ARC |
            PF_SETPOINT |
            PF_CURRENT |
            PF_ACTION |
            PF_BTN_MAIN1 |
            PF_SLIDER |
            PF_INFO |
            PF_BTN_NEXT |
            PF_BTN_PREV,

        .color = disp_color,

        .flameIcon = "",

        .actionText = DisplayText::BRIGHTNESS,

        .btnMain1Text = DisplayText::EN,
        .btnMain2Text = DisplayText::NONE,

        .nextPage = DisplayPage::HEATING,
        .prevPage = DisplayPage::DHW,

        .btnMain1Callback = display_language_cb,
        .sliderCallback = display_timeout_cb
    }
};

// LVGL objects
struct UIObjects
{
    lv_obj_t* screen = nullptr;

    lv_obj_t *root = nullptr;

    lv_obj_t *icon_flame = nullptr;
    lv_obj_t *icon_wifi = nullptr;
    lv_obj_t *icon_opentherm = nullptr;
    lv_obj_t *icon_mqtt = nullptr;
    lv_obj_t *time = nullptr;

    lv_obj_t* arc = nullptr;

    lv_obj_t* setpoint = nullptr;
    lv_obj_t* current = nullptr;
    lv_obj_t* action = nullptr;
    lv_obj_t* info = nullptr;

    lv_obj_t* btn_main1 = nullptr;
    lv_obj_t* btn_main2 = nullptr;

    lv_obj_t* btn_main1_label = nullptr;
    lv_obj_t* btn_main2_label = nullptr;

    lv_obj_t* btn_next = nullptr;
    lv_obj_t* btn_prev = nullptr;

    lv_obj_t* slider = nullptr;
};

static UIObjects ui;

static lv_style_t style_btn_off;
static lv_style_t style_btn_on;

static DisplayPage currentPage = DisplayPage::HEATING;

static PageValues g_pages[
    static_cast<uint8_t>(DisplayPage::COUNT)
];

static LastValues g_last;

// Helpers
// Temperature unit helper
static inline const char *temperatureUnit()
{
    return (settings.system.unitSystem ==
            UnitSystem::METRIC)
               ? "°C"
               : "°F";
}

static inline uint8_t pageIndex(DisplayPage page)
{
    return static_cast<uint8_t>(page);
}

static inline void setVisible(lv_obj_t* obj, bool visible)
{
    if (!obj)
    {
        return;
    }

    if (visible)
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }    
    else
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }    
}

struct
{
    bool on = true;
    bool to_save = false;
    uint32_t last_touch = 0;
    uint32_t last_save = 0;
    uint32_t last_collect = 0;
} display;

#if defined(DISPLAY_AHT20)
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

// Display functions
void displayOn()
{
    display.on = true;
    setBacklight(settings.display.brightness);
}

void displayOff()
{
    display.on = false;
    offBacklight();
}

// Callbacks - Events
static void heating_enable_cb(lv_event_t *e)
{
    settings.heating.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void heating_turbo_cb(lv_event_t *e)
{
    settings.heating.turbo = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void dhw_enable_cb(lv_event_t *e)
{
    settings.dhw.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void display_language_cb(lv_event_t *e)
{
    settings.display.language = static_cast<Language>((static_cast<uint8_t>(g_last.common.language) + 1) % 7);
}

static void display_timeout_cb(lv_event_t *e)
{
    settings.display.timeout_ms = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e)) * 1000;
    display.to_save = true;
}

static void page_prev_cb(lv_event_t *e)
{
    const PageLayout& layout = PAGE_LAYOUTS[pageIndex(currentPage)];
    renderPage(layout.prevPage);
}

static void page_next_cb(lv_event_t *e)
{
    const PageLayout& layout = PAGE_LAYOUTS[pageIndex(currentPage)];
    renderPage(layout.nextPage);
}

// Callbacks - ARC
static void arc_value_changed_cb(lv_event_t *e)
{
    lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);
    int value = lv_arc_get_value(arc);

    switch (currentPage)
    {
        // HEATING
        case DisplayPage::HEATING:
        {
            float temp = value / 10.0f;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f%s", temp, temperatureUnit());
            lv_label_set_text(ui.setpoint, buf);
            break;
        }

        // DHW
        case DisplayPage::DHW:
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%s", value, temperatureUnit());
            lv_label_set_text(ui.setpoint, buf);
            break;
        }

        // DISPLAY
        case DisplayPage::DISP:
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", value);
            lv_label_set_text(ui.setpoint, buf);
            break;
        }

        default:
            break;
    }
}

static void arc_released_cb(lv_event_t *e)
{
    lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);
    int value = lv_arc_get_value(arc);

    switch (currentPage)
    {
        // HEATING
        case DisplayPage::HEATING:
        {
            settings.heating.target = value / 10.0f;
            display.to_save = true;
            break;
        }

        // DHW
        case DisplayPage::DHW:
        {
            settings.dhw.target = value;
            display.to_save = true;
            break;
        }

        // DISPLAY
        case DisplayPage::DISP:
        {
            settings.display.brightness = value;
            setBacklight(settings.display.brightness);
            display.to_save = true;
            break;
        }

        default:
            break;
    }
}

// Callbacks - Main buttons
static void btn_main1_cb(lv_event_t *e)
{
    const PageLayout& layout = PAGE_LAYOUTS[pageIndex(currentPage)];
    if (layout.btnMain1Callback)
    {
        layout.btnMain1Callback(e);
    }
}

static void btn_main2_cb(lv_event_t *e)
{
    const PageLayout& layout = PAGE_LAYOUTS[pageIndex(currentPage)];
    if (layout.btnMain2Callback)
    {
        layout.btnMain2Callback(e);
    }
}

// Render Page layout
void renderPage(DisplayPage page)
{
    currentPage = page;

    const PageLayout& layout = PAGE_LAYOUTS[pageIndex(page)];
    PageValues& vals = g_pages[pageIndex(page)];

    // ---- VISIBILITY ----
    setVisible(ui.arc,        layout.flags & PF_ARC);
    setVisible(ui.setpoint,   layout.flags & PF_SETPOINT);
    setVisible(ui.current,    layout.flags & PF_CURRENT);
    setVisible(ui.btn_main1,  layout.flags & PF_BTN_MAIN1);
    setVisible(ui.btn_main2,  layout.flags & PF_BTN_MAIN2);
    setVisible(ui.btn_prev,   layout.flags & PF_BTN_PREV);
    setVisible(ui.btn_next,   layout.flags & PF_BTN_NEXT);
    setVisible(ui.slider,     layout.flags & PF_SLIDER);
    setVisible(ui.info,       layout.flags & PF_INFO);

    // ---- ARC ----
    if (layout.flags & PF_ARC)
    {
        lv_arc_set_range(ui.arc, vals.arcMinValue, vals.arcMaxValue);
        lv_arc_set_value(ui.arc, vals.arcValue);
        lv_obj_set_style_arc_color(ui.arc, layout.color, LV_PART_INDICATOR);
    }

    // ---- FLAME ----
    lv_label_set_text(ui.icon_flame, layout.flameIcon);
    lv_obj_set_style_text_color(ui.icon_flame, layout.color, 0);

    // ---- ACTION ----
    if (layout.flags & PF_ACTION)
    {
        lv_label_set_text(ui.action, tr(g_last.common.language, layout.actionText));
        lv_obj_set_style_text_color(ui.action, layout.color, 0);
        setVisible(ui.action, vals.active);
    }

    // ---- SETPOINT ----
    if (layout.flags & PF_SETPOINT)
    {
        switch (page)
        {
            case DisplayPage::HEATING:
                lv_label_set_text_fmt(ui.setpoint,"%d.%d%s", vals.arcValue / 10, abs(vals.arcValue % 10), temperatureUnit());
                break;

            case DisplayPage::DHW:
                lv_label_set_text_fmt(ui.setpoint, "%d%s", vals.arcValue, temperatureUnit());
                break;

            case DisplayPage::DISP:
                lv_label_set_text_fmt(ui.setpoint, "%d%%", vals.arcValue);
                break;

            default:
                break;
        }
    }

    // ---- CURRENT ----
    if (layout.flags & PF_CURRENT)
    {
        switch (page)
        {
            case DisplayPage::HEATING:
            case DisplayPage::DHW:
            {
                lv_obj_set_style_text_font(ui.current, &lv_font_montserrat_36, 0);
                lv_label_set_text_fmt(ui.current, "%d.%d%s", vals.currentValue / 10, abs(vals.currentValue % 10), temperatureUnit());
                break;
            }

            case DisplayPage::DISP:
            {
                lv_obj_set_style_text_font(ui.current, &lv_font_montserrat_24_ext, 0);
                lv_label_set_text_fmt(ui.current, "%s: %ds", tr(g_last.common.language, DisplayText::TIMEOUT), vals.currentValue);
                break;
            }

            default:
                break;
        }
        lv_obj_set_style_text_color(ui.current, layout.color, 0);
    }

    // ---- BUTTON MAIN1 ----
    if (layout.flags & PF_BTN_MAIN1)
    {
        if (page == DisplayPage::DISP)
        {
            lv_obj_add_state(ui.btn_main1, LV_STATE_CHECKED);
            lv_label_set_text(ui.btn_main1_label, tr(g_last.common.language, lt(g_last.common.language)));
        }
        else
        {
            if (vals.enabled)
            {
                lv_obj_add_state(ui.btn_main1, LV_STATE_CHECKED);
                lv_label_set_text(ui.btn_main1_label, tr(g_last.common.language, layout.btnMain1Text));
            }    
            else
            {
                lv_obj_clear_state(ui.btn_main1, LV_STATE_CHECKED);
                lv_label_set_text(ui.btn_main1_label, tr(g_last.common.language, layout.btnMain1Text));
            }    
        }
        lv_obj_set_style_bg_color(ui.btn_main1, layout.color, LV_STATE_CHECKED);
    }

    // ---- BUTTON MAIN2 ----
    if (layout.flags & PF_BTN_MAIN2)
    {
        lv_label_set_text(ui.btn_main2_label, tr(g_last.common.language, layout.btnMain2Text));
        lv_obj_set_style_bg_color(ui.btn_main2, layout.color, LV_STATE_CHECKED);
        if (vals.turbo)
        {
            lv_obj_add_state(ui.btn_main2, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui.btn_main2, LV_STATE_CHECKED);
        }
    }

    // ---- SLIDER ----
    if (layout.flags & PF_SLIDER)
    {
        lv_slider_set_range(ui.slider, 0, 120);
        lv_slider_set_value(ui.slider, vals.currentValue, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(ui.slider, layout.color, LV_PART_INDICATOR);
    }
}

// UI creation 
void initStyles()
{
    // ---- BUTTON OFF ----
    lv_style_init(&style_btn_off);
    lv_style_set_bg_opa(&style_btn_off, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_btn_off, 2);
    lv_style_set_border_color(&style_btn_off, gray_color);
    lv_style_set_text_color(&style_btn_off, lv_color_white());
    lv_style_set_radius(&style_btn_off, 10);

    // ---- BUTTON ON ----
    lv_style_init(&style_btn_on);
    lv_style_set_bg_opa(&style_btn_on, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_on, gray_color);
    lv_style_set_border_width(&style_btn_on, 2);
    lv_style_set_border_color(&style_btn_on, lv_color_white());
    lv_style_set_text_color(&style_btn_on, lv_color_white());
    lv_style_set_radius(&style_btn_on, 10);
}

void createUI()
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    initStyles();

    ui.root = lv_obj_create(scr);
    lv_obj_set_size(ui.root, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_pos(ui.root, 0, 0);

    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_set_style_border_width(ui.root, 0, 0);
    lv_obj_set_style_radius(ui.root, 0, 0);
    lv_obj_set_style_bg_color(ui.root, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ui.root, 0, 0);

    // ---- WIFI ----
    ui.icon_wifi = lv_label_create(ui.root);
    lv_label_set_text(ui.icon_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ui.icon_wifi, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.icon_wifi, gray_color, 0);
    lv_obj_align(ui.icon_wifi, LV_ALIGN_TOP_RIGHT, -9, 4);

    // ---- OT ----
    ui.icon_opentherm = lv_label_create(ui.root);
    lv_label_set_text(ui.icon_opentherm, "O");
    lv_obj_set_style_text_font(ui.icon_opentherm, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.icon_opentherm, gray_color, 0);
    lv_obj_align(ui.icon_opentherm, LV_ALIGN_TOP_RIGHT, -37, 4);

    // ---- MQTT ----
    ui.icon_mqtt = lv_label_create(ui.root);
    lv_label_set_text(ui.icon_mqtt, "M");
    lv_obj_set_style_text_font(ui.icon_mqtt, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(ui.icon_mqtt, gray_color, 0);
    lv_obj_align(ui.icon_mqtt, LV_ALIGN_TOP_RIGHT, -57, 4);

    // ---- FLAME ----
    ui.icon_flame = lv_label_create(ui.root);
    lv_obj_set_style_text_font(ui.icon_flame, &mdi_24, 0);
    lv_obj_align(ui.icon_flame, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_flag(ui.icon_flame, LV_OBJ_FLAG_HIDDEN);

    // ---- TIME ----
    ui.time = lv_label_create(ui.root);
    lv_label_set_text(ui.time, "--:--");
    lv_obj_set_style_text_font(ui.time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.time, lv_color_white(), 0);
    lv_obj_align(ui.time, LV_ALIGN_TOP_LEFT, 4, 4);

    // ---- ARC ----
    ui.arc = lv_arc_create(ui.root);
    lv_obj_set_size(ui.arc, (DISP_WIDTH * 7) / 8, (DISP_HEIGHT * 7) / 8);
    lv_obj_align(ui.arc, LV_ALIGN_CENTER, 0, -20);
    lv_arc_set_bg_angles(ui.arc, 135, 45);
    lv_obj_set_style_arc_width(ui.arc, 24, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.arc, arc_color, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.arc, 24, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui.arc, lv_color_white(), LV_PART_KNOB);

    lv_obj_add_event_cb(ui.arc, arc_value_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(ui.arc, arc_released_cb, LV_EVENT_RELEASED, nullptr);

    // ---- ACTION ----
    ui.action = lv_label_create(ui.root);
    lv_label_set_text(ui.action, "");
    lv_obj_set_style_text_font(ui.action, &lv_font_montserrat_24_ext, 0);
    lv_obj_align(ui.action, LV_ALIGN_CENTER, 0, -120);

    // ---- SETPOINT ----
    ui.setpoint = lv_label_create(ui.root);
    lv_label_set_text(ui.setpoint, "--.-°");
    lv_obj_set_style_text_font(ui.setpoint, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ui.setpoint, lv_color_white(), 0);
    lv_obj_align(ui.setpoint, LV_ALIGN_CENTER, 0, -20);

    // ---- CURRENT ----
    ui.current = lv_label_create(ui.root);
    lv_label_set_text(ui.current, "--.-°");
    lv_obj_align(ui.current, LV_ALIGN_CENTER, 0, 60);

    // ---- INFO ----
    ui.info = lv_label_create(ui.root);
    lv_label_set_text(ui.info, "");
    lv_obj_set_style_text_font(ui.info, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.info, gray_color, 0);
    lv_obj_align(ui.info, LV_ALIGN_BOTTOM_LEFT, 80, -20); 

    // ---- BUTTON MAIN1 ---- 
    ui.btn_main1 = lv_button_create(ui.root);
    lv_obj_set_size(ui.btn_main1, 100, 50);
    lv_obj_align(ui.btn_main1, LV_ALIGN_BOTTOM_RIGHT, -120, -5);
    lv_obj_add_flag(ui.btn_main1, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_add_style(ui.btn_main1, &style_btn_off, LV_PART_MAIN);
    lv_obj_add_style(ui.btn_main1, &style_btn_on, LV_STATE_CHECKED);    

    lv_obj_add_event_cb(ui.btn_main1, btn_main1_cb, LV_EVENT_CLICKED, nullptr);

    ui.btn_main1_label = lv_label_create(ui.btn_main1);
    lv_obj_set_style_text_font(ui.btn_main1_label, &lv_font_montserrat_20_ext, 0);
    lv_obj_center(ui.btn_main1_label);

    // ---- BUTTON MAIN2 ----
    ui.btn_main2 = lv_button_create(ui.root);
    lv_obj_set_size(ui.btn_main2, 100, 50);
    lv_obj_align(ui.btn_main2, LV_ALIGN_BOTTOM_RIGHT, -230, -5);
    lv_obj_add_flag(ui.btn_main2, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_add_style(ui.btn_main2, &style_btn_off, LV_PART_MAIN);
    lv_obj_add_style(ui.btn_main2, &style_btn_on, LV_STATE_CHECKED);

    lv_obj_add_event_cb(ui.btn_main2, btn_main2_cb, LV_EVENT_CLICKED, nullptr);

    ui.btn_main2_label = lv_label_create(ui.btn_main2);
    lv_obj_set_style_text_font(ui.btn_main2_label, &lv_font_montserrat_20_ext, 0);
    lv_obj_center(ui.btn_main2_label);

    // ---- PREV ----
    ui.btn_prev = lv_button_create(ui.root);
    lv_obj_set_size(ui.btn_prev, 54, 54);
    lv_obj_align(ui.btn_prev, LV_ALIGN_BOTTOM_LEFT, 1, -5);
    lv_obj_set_style_bg_opa(ui.btn_prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.btn_prev, 2, 0);
    lv_obj_set_style_border_color(ui.btn_prev, gray_color, 0);
    lv_obj_set_style_radius(ui.btn_prev, 27, 0);

    lv_obj_add_event_cb(ui.btn_prev, page_prev_cb, LV_EVENT_CLICKED, nullptr);

    {
        lv_obj_t *lbl = lv_label_create(ui.btn_prev);
        lv_label_set_text(lbl, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_center(lbl);
    }

    // ---- NEXT ----
    ui.btn_next = lv_button_create(ui.root);
    lv_obj_set_size(ui.btn_next, 54, 54);
    lv_obj_align(ui.btn_next, LV_ALIGN_BOTTOM_RIGHT, -1, -5);
    lv_obj_set_style_bg_opa(ui.btn_next, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.btn_next, 2, 0);
    lv_obj_set_style_border_color(ui.btn_next, gray_color, 0);
    lv_obj_set_style_radius(ui.btn_next, 27, 0);

    lv_obj_add_event_cb(ui.btn_next, page_next_cb, LV_EVENT_CLICKED, nullptr);

    {
        lv_obj_t *lbl = lv_label_create(ui.btn_next);
        lv_label_set_text(lbl, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_center(lbl);
    }

    // ---- SLIDER ----
    ui.slider  = lv_slider_create(ui.root);
    lv_obj_set_size(ui.slider, 200, 12);
    lv_obj_align(ui.slider, LV_ALIGN_CENTER, 0, 128);
    lv_slider_set_range(ui.slider, 0, 120);
    lv_obj_set_style_bg_color(ui.slider, slider_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.slider, lv_color_white(), LV_PART_KNOB);
 
    lv_obj_add_event_cb(ui.slider, display_timeout_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ---- INITIAL VALUES ----

    g_last.common.language = g_last.current.language;
}

// UI Update
void updateUI()
{
    const uint8_t idx = pageIndex(currentPage);

    PageValues& vals = g_pages[idx];
    PageValues& last = g_last.pages[idx];

    // ---- Language ----
    if (g_last.common.language != g_last.current.language)
    {
        g_last.common.language = g_last.current.language;
        renderPage(currentPage);
    }

    // ---- WIFI ----
    if (g_last.common.wifiConnected != g_last.current.wifiConnected )
    {
        g_last.common.wifiConnected = g_last.current.wifiConnected;
        lv_obj_set_style_text_color(ui.icon_wifi,
            g_last.common.wifiConnected
                ? lv_color_white()
                : gray_color,
            0);
        if (g_last.common.wifiConnected)
        {
            lv_label_set_text(ui.info, WiFi.localIP().toString().c_str());
        }    
        else
        {
            lv_label_set_text(ui.info, "");
        }    
    }

    // ---- MQTT ----
    if (g_last.common.mqttConnected != g_last.current.mqttConnected)
    {
        g_last.common.mqttConnected = g_last.current.mqttConnected;
        lv_obj_set_style_text_color(ui.icon_mqtt,
            g_last.common.mqttConnected
                ? lv_color_white()
                : gray_color,
            0);
    }

    // ---- OPENTHERM ----
    if (g_last.common.openthermConnected != g_last.current.openthermConnected)
    {
        g_last.common.openthermConnected = g_last.current.openthermConnected;
        lv_obj_set_style_text_color(ui.icon_opentherm,
            g_last.common.openthermConnected
                ? lv_color_white()
                : gray_color,
            0);
    }

    // ---- FLAME ----
    if (g_last.common.flame != g_last.current.flame)
    {
        g_last.common.flame = g_last.current.flame;
        setVisible(ui.icon_flame, g_last.common.flame);
    }

    // ---- TIME ----
    if (g_last.common.unixTime != g_last.current.unixTime)
    {
        g_last.common.unixTime = g_last.current.unixTime;
        const uint32_t timeValue = g_last.common.unixTime;
        const uint8_t hour = timeValue / 60;
        const uint8_t minute = timeValue % 60;
        char buf[6];
        snprintf(buf, sizeof(buf), "%02u:%02u", hour, minute);
        lv_label_set_text(ui.time, buf);
    }

    // ---- ARC RANGE ----
    if ((last.arcMinValue != vals.arcMinValue) || (last.arcMaxValue != vals.arcMaxValue))
    {
        last.arcMinValue = vals.arcMinValue;
        last.arcMaxValue = vals.arcMaxValue;
        lv_arc_set_range(ui.arc, last.arcMinValue,  last.arcMaxValue);
    }

    // ---- ARC VALUE ----
    if (last.arcValue != vals.arcValue)
    {
        last.arcValue = vals.arcValue;
        lv_arc_set_value(ui.arc, last.arcValue);

        switch (currentPage)
        {
            // HEATING
            case DisplayPage::HEATING:
            {
                lv_label_set_text_fmt(ui.setpoint, "%d.%d%s", last.arcValue / 10, abs(last.arcValue % 10), temperatureUnit());
                break;
            }

            // DHW
            case DisplayPage::DHW:
            {
                lv_label_set_text_fmt(ui.setpoint, "%d%s", last.arcValue, temperatureUnit());
                break;
            }

            // DISPLAY
            case DisplayPage::DISP:
            {
                lv_label_set_text_fmt(ui.setpoint, "%d%%", last.arcValue);
                break;
            }

            default:
                break;
        }
    }

    // ---- CURRENT ----
    if (last.currentValue != vals.currentValue)
    {
        last.currentValue = vals.currentValue;
        switch (currentPage)
        {
            // DISPLAY
            case DisplayPage::DISP:
            {
                lv_slider_set_value(ui.slider, last.currentValue, LV_ANIM_OFF);
                lv_label_set_text_fmt(ui.current, "%s: %ds", tr(g_last.common.language, DisplayText::TIMEOUT), last.currentValue);
                break;
            }

            // HEATING, DHW TEMP
            default:
            {
                lv_label_set_text_fmt(ui.current, "%d.%d%s", last.currentValue / 10, abs(last.currentValue % 10),  temperatureUnit());
                break;
            }
        }
    }

    // ---- ACTION ----
    if (last.active != vals.active)
    {
        last.active = vals.active;
        setVisible(ui.action, last.active);
    }

    // ---- ENABLED ----
    if (last.enabled != vals.enabled)
    {
        last.enabled = vals.enabled;
        if (last.enabled)
        {
            lv_obj_add_state(ui.btn_main1, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui.btn_main1, LV_STATE_CHECKED);
        }
    }

    // ---- TURBO ----
    if (last.turbo != vals.turbo)
    {
        last.turbo = vals.turbo;
        if (last.turbo)
        {
            lv_obj_add_state(ui.btn_main2, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(ui.btn_main2, LV_STATE_CHECKED);
        }
    }
}

// Data mining
static void collectPageValues()
{
    // ---- COMMON ----
    g_last.current.language = settings.display.language;
    g_last.current.wifiConnected = vars.network.connected;
    g_last.current.mqttConnected = vars.mqtt.connected;
    g_last.current.openthermConnected = vars.slave.connected;
    g_last.current.flame = vars.slave.flame;

    time_t now = time(nullptr);
    if (now > 100000)
    {
        struct tm timeinfo;
        if (localtime_r(&now, &timeinfo))
        {
            uint32_t timeValue = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
            g_last.current.unixTime = timeValue;
        }
    }

    // ---- HEATING ----
    {
        auto& page = g_pages[pageIndex(DisplayPage::HEATING)];

        page.arcValue = (int)(settings.heating.target * 10.0f);
        page.arcMinValue = settings.display.heating_minTemp10;
        page.arcMaxValue = settings.display.heating_maxTemp10;

        float currentTemp =
            vars.master.heating.indoorTempControl
                ? vars.master.heating.indoorTemp
                : vars.master.heating.currentTemp;
        page.currentValue = (int)(currentTemp * 10.0f);

        page.enabled = settings.heating.enabled;
        page.turbo = settings.heating.turbo;
        page.active = vars.slave.heating.active;
    }

    // ---- DHW ----
    {
        auto& page = g_pages[pageIndex(DisplayPage::DHW)];

        page.arcValue = (int)(settings.dhw.target);
        page.arcMinValue = settings.dhw.minTemp;
        page.arcMaxValue = settings.dhw.maxTemp;

        page.currentValue = (int)(vars.master.dhw.currentTemp * 10.0f);

        page.enabled = settings.dhw.enabled;
        page.active = vars.slave.dhw.active;
    }

    // ---- DISPLAY ----
    {
        auto& page = g_pages[pageIndex(DisplayPage::DISP)];

        page.arcValue = settings.display.brightness;
        page.arcMinValue = 0;
        page.arcMaxValue = 100;

        int timeout_sec = settings.display.timeout_ms / 1000;
        page.currentValue = timeout_sec;

        page.enabled = true;
        page.active = true;
    }
}

// Test display settings values
static void normalizeDisplaySettings()
{
    int heatTarget10 = (int)(settings.heating.target * 10.0f);
    if (settings.display.heating_minTemp10 > heatTarget10)
    {
        settings.display.heating_minTemp10 = max(0, heatTarget10 - 50);
    }
    if (settings.display.heating_maxTemp10 < heatTarget10)
    {
        settings.display.heating_maxTemp10 = heatTarget10 + 50;
    }
    if (settings.display.heating_minTemp10 >= settings.display.heating_maxTemp10)
    {
        settings.display.heating_minTemp10 = 0;
        settings.display.heating_maxTemp10 = 1000;
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
        settings.display.enabled = true;
        normalizeDisplaySettings();

#if defined(DISPLAY_AHT20)
        SensorAHT20.found = dt_aht20;
#endif

#if defined(DISPLAY_SPLASH_SCREEN)
        vTaskDelay(pdMS_TO_TICKS(settings.display.splash_time_ms)); 
#endif

        collectPageValues();
        createUI();
        renderPage(currentPage);
        updateUI();
        lv_timer_handler();
        lv_refr_now(NULL);
        display.last_touch = millis();
        touch.blocked = false;
        touch.wait_release = true;        
        displayOn();
    }

    void loop() override
    {
        if (vars.states.restarting || vars.states.upgrading)
        {
            return;
        }

#if defined(TOUCH_TYPE_GT911)
        touch.dev->read();
        touch.pressed = touch.dev->isTouched;
        if (touch.pressed)
        {
            touch.x = touch.dev->points[0].x;
            touch.y = touch.dev->points[0].y;
        }
#endif

#if defined(TOUCH_TYPE_FT6X36)
        touch.pressed = touch.dev->touched();
        if (touch.pressed)
            {
                auto p = touch.dev->getPoint();
                touch.x = p.x;
                touch.y = p.y;
            }
#endif

#if defined(TOUCH_TYPE_CST816)
        touch.pressed = touch.dev->available();
        if (touch.pressed)
        {
            touch.x = touch.dev->x;
            touch.y = touch.dev->y;
        }
#endif

        if (touch.pressed && !touch.last_state)
        {
            if (!display.on) 
            {
                setActivePriority();
                if (currentPage == DisplayPage::DISP)
                {
                    currentPage = DisplayPage::HEATING;
                    renderPage(currentPage);
                }
                updateUI();
                lv_timer_handler();
                touch.wait_release = true;
                displayOn();
                return;                
            }
            display.last_touch = millis();
        }
        touch.last_state = touch.pressed;

#if defined(DISPLAY_AHT20)
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

        if ((millis() - display.last_collect) > settings.display.collect_time_ms)
        {
                display.last_collect = millis();
                collectPageValues();
        }

        if (!(settings.display.timeout_ms == 0))
        {
            if (display.on && (millis() - display.last_touch > settings.display.timeout_ms))
            {
                displayOff();
                setSleepPriority();
            }
        }

        if (!display.on) return;

        updateUI();
        lv_timer_handler();
    }
};