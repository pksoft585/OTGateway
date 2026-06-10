#pragma once

#include <lvgl.h>
#include "dt_local.h"

#if defined(DISPLAY_TYPE_DIYLESS3)
#include "dt_diyless3.h"
#endif

#if defined(DISPLAY_TYPE_GUITION)
#include "dt_guition.h"
#endif

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
 }

// Pages
// Forward declarations
static void heating_enable_cb(lv_event_t *e);
static void heating_turbo_cb(lv_event_t *e);
static void dhw_enable_cb(lv_event_t *e);
static void display_language_cb(lv_event_t *e);
static void display_timeout_cb(lv_event_t *e);

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
