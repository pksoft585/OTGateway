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

const char* wifi_ip_text = "";
const char* temperatureUnit = "°C";

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
    
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        if(lv_event_get_indev(e) == NULL) return; 

        g_pages[pageIndex(DisplayPage::DISP)].currentValue = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
        display.timeout_ms = g_pages[pageIndex(DisplayPage::DISP)].currentValue * 1000;
        display.to_save = true;
    }
}

// Callback - ARC
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

// Data mining
static void collectPageValues()
{
    // ---- COMMON ----
    espnow_settings.unitSys = espnow_therm.common.unitSys;
    temperatureUnit = (espnow_settings.unitSys == 0) ? "°C" : "°F";

    g_last.current.language = espnow_therm.common.language;
    g_last.current.wifiConnected = espnow_therm.common.wifiConnected;

    espnow_settings.wifiIp[0] = espnow_therm.common.wifiIp[0];
    espnow_settings.wifiIp[1] = espnow_therm.common.wifiIp[1];
    espnow_settings.wifiIp[2] = espnow_therm.common.wifiIp[2];
    espnow_settings.wifiIp[3] = espnow_therm.common.wifiIp[3];

    static String ipText;

    if (g_last.current.wifiConnected)
    {
        ipText = String(espnow_settings.wifiIp[0]) + "." + 
                 String(espnow_settings.wifiIp[1]) + "." + 
                 String(espnow_settings.wifiIp[2]) + "." + 
                 String(espnow_settings.wifiIp[3]);
        wifi_ip_text = ipText.c_str();
    }    
    else
    {
        wifi_ip_text = "";
    }    

    g_last.current.mqttConnected = espnow_therm.common.mqttConnected;
    g_last.current.openthermConnected = espnow_therm.common.openthermConnected;
    g_last.current.flame = espnow_therm.common.flame;

    g_last.current.unixTime = espnow_therm.common.unixTime;

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
/*    
        Serial.printf("  >>> HEATING Raw - arcVal: %d, min: %d, max: %d, curVal: %d, en: %d, turbo: %d, act: %d\n",
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue,
            page.enabled ? 1 : 0,
            page.turbo ? 1 : 0,
            page.active ? 1 : 0
        );
*/    
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
/*    
        Serial.printf("  >>> DHW Raw - arcVal: %d, min: %d, max: %d, curVal: %d, en: %d, act: %d\n",
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue,
            page.enabled ? 1 : 0,
            page.active ? 1 : 0
        );
*/    
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
/*    
        Serial.printf("  >>> DISP Raw - arcVal: %d, min: %d, max: %d, curVal: %d\n",
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue
        );
*/    
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
/*    
    Serial.printf("  >>> Display data - HeatTgt: %d, HeatEn: %d, HeatTu: %d, DhwTgt: %d, DhwEn: %d, BackTgt: %d, DisTim: %d, act: %d\n",
        espnow_disp.heatingTarget,
        espnow_disp.heatingEnabled ? 1 : 0,
        espnow_disp.heatingTurbo ? 1 : 0,
        espnow_disp.dhwTarget,
        espnow_disp.dhwEnabled ? 1 : 0,
        espnow_disp.backlightTarget,
        espnow_disp.dispTimeout,
        espnow_disp.language
    );
*/    
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
        temperatureUnit = (espnow_settings.unitSys == 0) ? "°C" : "°F";

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
        renderPage(currentPage);
        updateUI();
        lv_timer_handler();
        lv_refr_now(NULL);
        display.last_touch = millis();
        touch.blocked = false;
        touch.wait_release = true;        
        setBacklight(BACKLIGHT_DEFAULT);
        display.on = true;
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
            if (!display.on) 
            {
                setActivePriority();
                display.last_touch = current_time;
                touch.wait_release = true; 
                displayOn();
                return;                
            }
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