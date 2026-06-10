#include <LeanTask.h>

#include "display/dt_local.h"
#include "display/dt_pages.h"

#if defined(DISPLAY_TYPE_DIYLESS3)
#include "display/dt_st7701.h"
#include "display/dt_ui.h"
#endif

#if defined(DISPLAY_TYPE_GUITION)
#include "display/dt_st7701.h"
#include "display/dt_ui.h"
#endif

#if defined(DISPLAY_TYPE_ESPNOW)
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "display/dt_espnow.h"
#include "display/espnow_data.h"
#endif

// Externed vars/settings
extern Variables vars;
extern Settings settings;

struct
{
    bool on = true;
    bool to_save = false;
    uint32_t last_touch = 0;
    uint32_t last_save = 0;
    uint32_t last_collect = 0;
    uint32_t last_heartbeat = 0;
} display;

#if defined(DISPLAY_TYPE_ESPNOW)

//  Remote Display

// Forward declarations
static bool collectEspnowValues();

volatile bool flag_data_received = false;

void addPeer(const uint8_t *mac, uint8_t channel)
{
    if (esp_now_is_peer_exist(mac)) return;

    esp_now_peer_info_t peerInfo = {}; 
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;

    wifi_mode_t mode = WiFi.getMode();
    
    if (mac[0] == 0xFF)
    {
        peerInfo.ifidx = WIFI_IF_AP;
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Broadcast peer on AP interface."));
    }
    else
    {
        if (mode == WIFI_MODE_APSTA)
        {
            peerInfo.ifidx = WIFI_IF_AP;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Unicast peer on AP interface."));
        }
        else if (mode == WIFI_MODE_AP)
        {
            peerInfo.ifidx = WIFI_IF_AP;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Unicast peer on AP interface."));
        }
        else
        {
            peerInfo.ifidx = WIFI_IF_STA;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Unicast peer on STA interface."));
        }
    }

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Add peer error!"));
    }
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
    if (len < 1) return;
    uint8_t type = incomingData[0];
    uint32_t current_time = millis();

/*
    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Raw data length: %d, type: %d."), len, type);
*/    
    switch (type) 
    {
        case DATA_DISP: 
            if (len == sizeof(displayValues))
            {
                memcpy(&espnow_disp, incomingData, sizeof(displayValues));
                Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display data received."));
                espnow.last_data = current_time;
                espnow.received = true;
            }
            break;
    
        case PAIR_REQUEST:
            if (len == sizeof(pairingData))
            {
                memcpy(&espnow_pairing, incomingData, sizeof(pairingData));
                Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Pairing request."));

                if (espnow_pairing.id > 0)
                {
                    memcpy(displayMac, recv_info->src_addr, 6);
                    displayChannel = espnow_pairing.channel;

                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display MAC: %02X:%02X:%02X:%02X:%02X:%02X."), 
                                displayMac[0], displayMac[1], displayMac[2], 
                                displayMac[3], displayMac[4], displayMac[5]);
                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display channel: %d."), displayChannel);
                    
                    WiFi.setChannel(displayChannel, WIFI_SECOND_CHAN_NONE);
                    esp_now_del_peer(displayMac);

                    addPeer(displayMac, displayChannel);

                    espnow.last_data = current_time;

                    espnow_pairing.msgType = PAIR_RESPONSE;
                    espnow_pairing.id = 0;
                    memcpy(espnow_pairing.macAddr, thermostatMac, 6);
                    espnow_pairing.channel = thermostatChannel;

                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Pairing response sending..."));
                    esp_err_t result = esp_now_send(displayMac, (uint8_t *) &espnow_pairing, sizeof(pairingData));
                    if (result == ESP_OK)
                    {
                        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Pairing OK."));
                        espnow.paired = true;
                        collectEspnowValues();
                        espnow.to_sent = true;
                    }
                    else
                    {
                        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Pairing response error: %d."), result);
                    }     
                }
            }  
            break; 
    }
}

bool init_esp_now()
{
    WiFi.macAddress(thermostatMac);

    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Thermostat MAC: %02X:%02X:%02X:%02X:%02X:%02X."), 
                thermostatMac[0], thermostatMac[1], thermostatMac[2], 
                thermostatMac[3], thermostatMac[4], thermostatMac[5]);
                                
    if (esp_now_init() != ESP_OK)
    {
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Initialization failed!"));
        return false;
    }

    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: WiFi mode: %d."), WiFi.getMode());
    
    if (vars.network.connected)
    {
        thermostatChannel = WiFi.channel();
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Wi-Fi channel (from connected): %d."), thermostatChannel);
    }
    else 
    {
        wifi_mode_t t_mode = (wifi_mode_t)WiFi.getMode();
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: WiFi mode enum: %d."), t_mode);

        if (t_mode == WIFI_MODE_AP || t_mode == WIFI_MODE_APSTA) 
        {
            thermostatChannel = WiFi.channel();
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Wi-Fi channel (from AP channel): %d."), thermostatChannel);
        } 
        else 
        {
            esp_err_t chan_err = esp_wifi_set_channel(thermostatChannel, WIFI_SECOND_CHAN_NONE);
            if (chan_err != ESP_OK)
            {
                Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Channel set error: %d."), chan_err);
                esp_now_deinit();
                return false;
            }
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Wi-Fi channel (set): %d."), thermostatChannel);
        }
    }

    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    addPeer(broadcastMac, thermostatChannel);

    espnow.initialized = true;

    return true;
}

// NetworkMgr hooks
namespace NetworkUtils {
    void espnow_deinit_hook()
    {
        if (espnow.initialized)
        {
            esp_now_register_recv_cb(NULL);
            esp_now_deinit();
            espnow.initialized = false;
            espnow.paired = false;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Deinitialized due to Wi-Fi mode change."));
        }
    }

    void espnow_reinit_hook(uint8_t channel)
    {
        thermostatChannel = channel; 
        init_esp_now();
    }
}

// Data mining
static bool collectEspnowValues()
{
    static thermostatValues last_espnow_therm;
    static uint32_t last_forced_send_time = 0;
    bool timeout_reached = false;

    // ---- COMMON ----
    espnow_therm.common.language = settings.display.language;
    espnow_therm.common.wifiConnected = vars.network.connected;
    espnow_therm.common.mqttConnected = vars.mqtt.connected;
    espnow_therm.common.openthermConnected = vars.slave.connected;
    espnow_therm.common.flame = vars.slave.flame;
    if (settings.system.unitSystem == UnitSystem::METRIC)
    {
        espnow_therm.common.unitSys = 0;
    }
    else
    {
        espnow_therm.common.unitSys = 1;
    }

    if (vars.network.connected)
    {
        IPAddress ip = WiFi.localIP();
        for (int i = 0; i < 4; i++) {
            espnow_therm.common.wifiIp[i] = ip[i];
        }

        if (!espnow.initialized)
        {
            espnow.initialized = init_esp_now();
        }
    }    
    else
    {
        for (int i = 0; i < 4; i++) {
            espnow_therm.common.wifiIp[i] = 0;
        }
    }    

    time_t now = time(nullptr);
    if (now > 100000)
    {
        struct tm timeinfo;
        if (localtime_r(&now, &timeinfo))
        {
            uint32_t timeValue = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
            espnow_therm.common.unixTime = timeValue;
        }
    }

    // ---- HEATING ----
    {
        auto& page = espnow_therm.pages[pageIndex(DisplayPage::HEATING)];

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
        
/*
        Log.sinfoln(FPSTR(L_DISPLAY), 
            F("  >>> HEATING Raw - arcVal: %d, min: %d, max: %d, curVal: %d, en: %d, turbo: %d, act: %d."),
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
        auto& page = espnow_therm.pages[pageIndex(DisplayPage::DHW)];

        page.arcValue = (int)(settings.dhw.target);
        page.arcMinValue = settings.dhw.minTemp;
        page.arcMaxValue = settings.dhw.maxTemp;

        page.currentValue = (int)(vars.master.dhw.currentTemp * 10.0f);

        page.enabled = settings.dhw.enabled;
        page.active = vars.slave.dhw.active;


/*
        Log.sinfoln(FPSTR(L_DISPLAY), 
            F("  >>> DHW Raw - arcVal: %d, min: %d, max: %d, curVal: %d, en: %d, act: %d."),
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
        auto& page = espnow_therm.pages[pageIndex(DisplayPage::DISP)];

        page.arcValue = settings.display.brightness;
        page.arcMinValue = 0;
        page.arcMaxValue = 100;

        int timeout_sec = settings.display.timeout_ms / 1000;
        page.currentValue = timeout_sec;

        page.enabled = true;
        page.active = true;


/*
        Log.sinfoln(FPSTR(L_DISPLAY), 
            F("  >>> DISP Raw - arcVal: %d, min: %d, max: %d, curVal: %d."),
            page.arcValue,
            page.arcMinValue,
            page.arcMaxValue,
            page.currentValue
        );
*/    
    }

    // ---- Test for changes or timeout ---
    if (millis() - last_forced_send_time >= 60000)
    {
        timeout_reached = true;
    }
    
    if (memcmp(&espnow_therm, &last_espnow_therm, sizeof(thermostatValues)) != 0 || timeout_reached)
    {
        memcpy(&last_espnow_therm, &espnow_therm, sizeof(thermostatValues));
        last_forced_send_time = millis(); 
        return true;
    }

    return false;
}

// Set display data
static void storeEspnowValues()
{
    float temp = espnow_disp.heatingTarget / 10.0f;
    settings.heating.target = temp;
    settings.dhw.target = espnow_disp.dhwTarget;
    settings.display.brightness = espnow_disp.backlightTarget;
    settings.heating.enabled = espnow_disp.heatingEnabled;
    settings.heating.turbo = espnow_disp.heatingTurbo;
    settings.dhw.enabled = espnow_disp.dhwEnabled;
    settings.display.language = espnow_disp.language;
    settings.display.timeout_ms = espnow_disp.dispTimeout;

    display.to_save = true;
    
/*
    Log.sinfoln(FPSTR(L_DISPLAY), 
        F("  >>> Display Raw - HeatTgt: %d, HeatEn: %d, HeatTu: %d, DhwTgt: %d, DhwEn: %d, BackTgt: %d, DisTim: %d, act: %d."),
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

#else

// Local Display

// Forward declarations
void renderPage(DisplayPage page);

// Helpers
// Temperature unit helper
static inline const char *temperatureUnit()
{
    return (settings.system.unitSystem ==
            UnitSystem::METRIC)
               ? "°C"
               : "°F";
}

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

#endif

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

#if defined(DISPLAY_TYPE_ESPNOW)
//  Remote Display
    void setup() override
    {
        settings.display.enabled = true;
        normalizeDisplaySettings();
        thermostatChannel = networkSettings.ap.channel; 
        if (thermostatChannel == 0 || thermostatChannel > 13)
        {
            thermostatChannel = 6; 
        }
    }

    void loop() override
    {
        uint32_t current_time = millis();

        if (current_time - display.last_heartbeat >= espnow.hearbeatTime)
        {
            display.last_heartbeat = current_time;

            if (espnow.paired)
            {
                esp_err_t result = esp_now_send(displayMac, (uint8_t *)&espnow_heartbeat, sizeof(heartbeatData));
                if (result != ESP_OK)
                {
                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Heartbeat sending error: %d."), result);
                }
            }
        }

        if ((current_time - display.last_collect) > settings.display.collect_time_ms)
        {
            display.last_collect = current_time;
            if (espnow.paired && collectEspnowValues())
            {
                espnow.to_sent = true;
            }
        }

        if (espnow.initialized)
        {
            if (espnow.received)
            {
                espnow.received = false;
                storeEspnowValues();
            }

            if ((current_time - display.last_save) > settings.display.save_time_ms)
            {
                display.last_save = current_time;
                if (display.to_save)
                {
                    fsSettings.update();
                    display.to_save = false;
                }
            }

            if (espnow.to_sent && !display.to_save)
            {
                espnow.to_sent = false;
                esp_err_t result = esp_now_send(displayMac, (uint8_t *)&espnow_therm, sizeof(thermostatValues));
                
                if (result != ESP_OK)
                {
                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Data sending error: %d."), result);
                }
            }
        }
    }    

#else

//  Local Display
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


#if defined(DISPLAY_AHT20)
        if (SensorAHT20.read)
        {
            if ((current_time - SensorAHT20.last_read) > SensorAHT20.period)
            {
                SensorAHT20.last_read = current_time;
                update_aht20();
            }
        }
#endif

        if ((current_time - display.last_save) > settings.display.save_time_ms)
        {
            display.last_save = current_time;
            if (display.to_save)
            {
                fsSettings.update();
                display.to_save = false;
            }
        }

        if ((current_time - display.last_collect) > settings.display.collect_time_ms)
        {
            display.last_collect = current_time;
            collectPageValues();
        }

        if (!(settings.display.timeout_ms == 0))
        {
            if (display.on && (current_time - display.last_touch > settings.display.timeout_ms))
            {
                displayOff();
                setSleepPriority();
            }
        }

        if (!display.on) return;

        updateUI();
        lv_timer_handler();
    }

#endif

};