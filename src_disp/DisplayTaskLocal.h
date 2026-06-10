#include <LeanTask.h>

#include "display/dt_local.h"
#include "display/dt_pages.h"
#include "display/espnow_data.h"

#if defined(DISPLAY_TYPE_GUITION)
#include "display/dt_st7701.h"
#include "display/dt_ui.h"
#endif

struct
{
    bool on = true;
    bool to_save = false;
    uint32_t last_touch = 0;
    uint32_t last_save = 0;
    uint32_t last_collect = 0;
    uint32_t timeout_ms = 30000; 
    uint32_t save_time_ms= 3000; 
    uint32_t splash_time_ms= 3000;
} display;

// Forward declarations
void renderPage(DisplayPage page);

// Helpers
// Temperature unit helper
static inline const char *temperatureUnit()
{
    return (espnow_settings.unitSys == 0)
               ? "°C"
               : "°F";
}

// Display functions
void displayOn()
{
    display.on = true;
    setBacklight(g_pages[pageIndex(DisplayPage::DISP)].arcValue);
}

void displayOff()
{
    display.on = false;
    offBacklight();
}

// Callbacks - Events
static void heating_enable_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    g_pages[pageIndex(DisplayPage::HEATING)].enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void heating_turbo_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    g_pages[pageIndex(DisplayPage::HEATING)].turbo = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void dhw_enable_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    g_pages[pageIndex(DisplayPage::DHW)].enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void display_language_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    g_last.current.language = static_cast<Language>((static_cast<uint8_t>(g_last.common.language) + 1) % 7);
    display.to_save = true;
}

static void display_timeout_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_VALUE_CHANGED) {
        if(lv_event_get_indev(e) == NULL) return; 

        g_pages[pageIndex(DisplayPage::DISP)].currentValue = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
        display.timeout_ms = g_pages[pageIndex(DisplayPage::DISP)].currentValue * 1000;
        display.to_save = true;
    }
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
    if(lv_event_get_indev(e) == NULL) return;

    lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);
    int value = lv_arc_get_value(arc);

    switch (currentPage)
    {
        // HEATING
        case DisplayPage::HEATING:
        {
            g_pages[pageIndex(DisplayPage::HEATING)].arcValue = value;
            display.to_save = true;
            break;
        }

        // DHW
        case DisplayPage::DHW:
        {
            g_pages[pageIndex(DisplayPage::DHW)].arcValue = value;
            display.to_save = true;
            break;
        }

        // DISPLAY
        case DisplayPage::DISP:
        {
            setBacklight(value);
            g_pages[pageIndex(DisplayPage::DISP)].arcValue = value;
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

    // ---- Sync ----
//    g_last.pages[pageIndex(page)] = vals; 
}

// UI add events 
void eventsUI()
{
    // ---- ARC ----
    lv_obj_add_event_cb(ui.arc, arc_value_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(ui.arc, arc_released_cb, LV_EVENT_RELEASED, nullptr);

    // ---- BUTTON MAIN1 ---- 
    lv_obj_add_event_cb(ui.btn_main1, btn_main1_cb, LV_EVENT_CLICKED, nullptr);

    // ---- BUTTON MAIN2 ----
    lv_obj_add_event_cb(ui.btn_main2, btn_main2_cb, LV_EVENT_CLICKED, nullptr);

    // ---- PREV ----
    lv_obj_add_event_cb(ui.btn_prev, page_prev_cb, LV_EVENT_CLICKED, nullptr);

    // ---- NEXT ----
    lv_obj_add_event_cb(ui.btn_next, page_next_cb, LV_EVENT_CLICKED, nullptr);

    // ---- SLIDER ----
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
            lv_label_set_text_fmt(
                ui.info, 
                "%d.%d.%d.%d", 
                espnow_settings.wifiIp[0],
                espnow_settings.wifiIp[1],
                espnow_settings.wifiIp[2],
                espnow_settings.wifiIp[3]
            );
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
    g_last.current.language = espnow_therm.common.language;
    g_last.current.wifiConnected = espnow_therm.common.wifiConnected;
    g_last.current.mqttConnected = espnow_therm.common.mqttConnected;
    g_last.current.openthermConnected = espnow_therm.common.openthermConnected;
    g_last.current.flame = espnow_therm.common.flame;
    g_last.current.unixTime = espnow_therm.common.unixTime;

    espnow_settings.unitSys = espnow_therm.common.unitSys;
    espnow_settings.wifiIp[0] = espnow_therm.common.wifiIp[0];
    espnow_settings.wifiIp[1] = espnow_therm.common.wifiIp[1];
    espnow_settings.wifiIp[2] = espnow_therm.common.wifiIp[2];
    espnow_settings.wifiIp[3] = espnow_therm.common.wifiIp[3];

    // ---- HEATING ----
    {
        auto& page = g_pages[pageIndex(DisplayPage::HEATING)];

        page.arcValue = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].arcValue;
        page.arcMinValue = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].arcMinValue;
        page.arcMaxValue = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].arcMaxValue;
        page.currentValue = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].currentValue;
        page.enabled = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].enabled;
        page.turbo = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].turbo;
        page.active = espnow_therm.pages[pageIndex(DisplayPage::HEATING)].active;

        Serial.printf("  >>> HEATING Raw - arcVal: %d, min: %d, max: %d, curVal: %d, en: %d, turbo: %d, act: %d.\n",
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue,
            page.enabled ? 1 : 0,
            page.turbo ? 1 : 0,
            page.active ? 1 : 0
        );
    }

    // ---- DHW ----
    {
        auto& page = g_pages[pageIndex(DisplayPage::DHW)];

        page.arcValue = espnow_therm.pages[pageIndex(DisplayPage::DHW)].arcValue;
        page.arcMinValue = espnow_therm.pages[pageIndex(DisplayPage::DHW)].arcMinValue;
        page.arcMaxValue = espnow_therm.pages[pageIndex(DisplayPage::DHW)].arcMaxValue;
        page.currentValue = espnow_therm.pages[pageIndex(DisplayPage::DHW)].currentValue;
        page.enabled = espnow_therm.pages[pageIndex(DisplayPage::DHW)].enabled;
        page.active = espnow_therm.pages[pageIndex(DisplayPage::DHW)].active;

        Serial.printf("  >>> DHW Raw - arcVal: %d, min: %d, max: %d, curVal: %d, en: %d, act: %d.\n",
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue,
            page.enabled ? 1 : 0,
            page.active ? 1 : 0
        );
    }

    // ---- DISPLAY ----
    {
        auto& page = g_pages[pageIndex(DisplayPage::DISP)];

        page.arcValue = espnow_therm.pages[pageIndex(DisplayPage::DISP)].arcValue;
        page.arcMinValue = 0;
        page.arcMaxValue = 100;
        page.currentValue = espnow_therm.pages[pageIndex(DisplayPage::DISP)].currentValue;
        page.enabled = true;
        page.active = true;
        display.timeout_ms = page.currentValue * 1000;

        Serial.printf("  >>> DISP Raw - arcVal: %d, min: %d, max: %d, curVal: %d.\n",
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue
        );
    }
}

static void collectDisplayValues()
{
        espnow_disp.heatingTarget = g_pages[pageIndex(DisplayPage::HEATING)].arcValue;
        espnow_disp.dhwTarget = g_pages[pageIndex(DisplayPage::DHW)].arcValue;
        espnow_disp.backlightTarget = g_pages[pageIndex(DisplayPage::DISP)].arcValue;
        espnow_disp.heatingEnabled = g_pages[pageIndex(DisplayPage::HEATING)].enabled;
        espnow_disp.heatingTurbo = g_pages[pageIndex(DisplayPage::HEATING)].turbo;
        espnow_disp.dhwEnabled = g_pages[pageIndex(DisplayPage::DHW)].enabled;
        espnow_disp.language = g_last.common.language;
        espnow_disp.dispTimeout = g_pages[pageIndex(DisplayPage::DISP)].currentValue * 1000;
    
    Serial.printf("  >>> Display data - HeatTgt: %d, HeatEn: %d, HeatTu: %d, DhwTgt: %d, DhwEn: %d, BackTgt: %d, DisTim: %d, act: %d.\n",
        espnow_disp.heatingTarget,
        espnow_disp.heatingEnabled ? 1 : 0,
        espnow_disp.heatingTurbo ? 1 : 0,
        espnow_disp.dhwTarget,
        espnow_disp.dhwEnabled ? 1 : 0,
        espnow_disp.backlightTarget,
        espnow_disp.dispTimeout,
        espnow_disp.language
    );
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

    BaseType_t getTaskCore() override 
    {
        return 0;
    }

    int getTaskPriority() override
    {
        return ACTIVE_PRIORITY;
    }
#endif

private:
    void setup() override
    {

#if defined(DISPLAY_SPLASH_SCREEN)
        vTaskDelay(pdMS_TO_TICKS(display.splash_time_ms)); 
#endif

        displayGrayscaleMode = !espnow.paired;
        if (!espnow.received)
        {
            defaultPageValues();
            collectPageValues();
        }
        else
        {
            espnow.received = false;
            collectPageValues();

        }
        createUI();
        eventsUI();
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
        uint32_t current_time = millis();
 
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
                display.last_touch = current_time;
                touch.wait_release = true; 
                updateUI();
                lv_timer_handler();
                displayOn();
                touch.last_state = touch.pressed;
                return;                
            }
            display.last_touch = current_time;
        }
        touch.last_state = touch.pressed;

        if (displayGrayscaleMode != !espnow.paired)
        {
            displayGrayscaleMode = !espnow.paired; 
            setDisplayGrayscale(displayGrayscaleMode);
        }

        if (espnow.received)
        {
            espnow.received = false;
            collectPageValues();
        }

        if ((current_time - display.last_save) > display.save_time_ms)
        {
            display.last_save = current_time;
            if (display.to_save && !espnow.to_sent)
            {
                collectDisplayValues();
                espnow.to_sent = true;
                display.to_save = false;
            }
        }

        if (!(display.timeout_ms == 0))
        {
            if (display.on && (current_time - display.last_touch > display.timeout_ms))
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