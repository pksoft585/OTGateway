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
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Broadcast peer on AP interface"));
    }
    else
    {
        if (mode == WIFI_MODE_APSTA)
        {
            peerInfo.ifidx = WIFI_IF_AP;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Unicast peer on AP interface"));
        }
        else if (mode == WIFI_MODE_AP)
        {
            peerInfo.ifidx = WIFI_IF_AP;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Unicast peer on AP interface"));
        }
        else
        {
            peerInfo.ifidx = WIFI_IF_STA;
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Unicast peer on STA interface"));
        }
    }

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Log.serrorln(FPSTR(L_DISPLAY), F("Esp-now: Add peer error!"));
    }
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
    if (len < 1) return;
    uint8_t type = incomingData[0];
    uint32_t current_time = millis();

/*
    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Raw data - length: %d, type: %d"), len, type);
*/    
    switch (type) 
    {
        case DATA_DISP: 
            if (len == sizeof(displayValues))
            {
                memcpy(&espnow_disp, incomingData, sizeof(displayValues));
                Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display data received"));
                espnow.last_data = current_time;
                espnow.received = true;
            }
            break;
    
        case PAIR_REQUEST:
            if (len == sizeof(pairingData))
            {
                memcpy(&espnow_pairing, incomingData, sizeof(pairingData));
                Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Pairing request received"));

                if (espnow_pairing.id > 0)
                {
                    memcpy(displayMac, recv_info->src_addr, 6);
                    displayChannel = espnow_pairing.channel;

                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display MAC: %02X:%02X:%02X:%02X:%02X:%02X"), 
                                displayMac[0], displayMac[1], displayMac[2], 
                                displayMac[3], displayMac[4], displayMac[5]);
                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display channel: %d"), displayChannel);
                    
                    WiFi.setChannel(displayChannel, WIFI_SECOND_CHAN_NONE);
                    esp_now_del_peer(displayMac);

                    addPeer(displayMac, displayChannel);

                    espnow.last_data = current_time;

                    espnow_pairing.msgType = PAIR_RESPONSE;
                    espnow_pairing.id = 0;
                    memcpy(espnow_pairing.macAddr, thermostatMac, 6);
                    espnow_pairing.channel = thermostatChannel;

//                    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Pairing response sending..."));
                    esp_err_t result = esp_now_send(displayMac, (uint8_t *) &espnow_pairing, sizeof(pairingData));
                    if (result == ESP_OK)
                    {
                        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Display paired"));
                        espnow.paired = true;
                        collectEspnowValues();
                        espnow.to_sent = true;
                    }
                    else
                    {
                        Log.serrorln(FPSTR(L_DISPLAY), F("Esp-now: Pairing response sending error: %d!"), result);
                    }     
                }
            }  
            break; 
    }
}

bool init_esp_now()
{
    WiFi.macAddress(thermostatMac);

    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Thermostat MAC: %02X:%02X:%02X:%02X:%02X:%02X"), 
                thermostatMac[0], thermostatMac[1], thermostatMac[2], 
                thermostatMac[3], thermostatMac[4], thermostatMac[5]);
                                
    if (esp_now_init() != ESP_OK)
    {
        Log.serrorln(FPSTR(L_DISPLAY), F("Esp-now: Initialization failed!"));
        return false;
    }

    wifi_mode_t t_mode = (wifi_mode_t)WiFi.getMode();
    Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: WiFi mode: %d"), t_mode);

    if (vars.network.connected)
    {
        thermostatChannel = WiFi.channel();
        Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Wi-Fi channel (STA): %d"), thermostatChannel);
    }
    else 
    {
        if (t_mode == WIFI_MODE_AP || t_mode == WIFI_MODE_APSTA) 
        {
            thermostatChannel = WiFi.channel();
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Wi-Fi channel (AP): %d"), thermostatChannel);
        } 
        else 
        {
            esp_err_t chan_err = esp_wifi_set_channel(thermostatChannel, WIFI_SECOND_CHAN_NONE);
            if (chan_err != ESP_OK)
            {
                Log.serrorln(FPSTR(L_DISPLAY), F("Esp-now: Channel set error: %d!"), chan_err);
                esp_now_deinit();
                return false;
            }
            Log.sinfoln(FPSTR(L_DISPLAY), F("Esp-now: Wi-Fi channel (Set): %d"), thermostatChannel);
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
            Log.swarningln(FPSTR(L_DISPLAY), F("Esp-now: Deinitialized due to Wi-Fi mode change"));
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
}

#else

// Local Display
const char* wifi_ip_text = "";
const char* temperatureUnit = "°C";

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
    if(lv_event_get_indev(e) == NULL) return;

    settings.heating.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void heating_turbo_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    settings.heating.turbo = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void dhw_enable_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    settings.dhw.enabled = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
    display.to_save = true;
}

static void display_language_cb(lv_event_t *e)
{
    if(lv_event_get_indev(e) == NULL) return;

    settings.display.language = static_cast<Language>((static_cast<uint8_t>(g_last.common.language) + 1) % 7);
}

static void display_timeout_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_VALUE_CHANGED)
    {
        if(lv_event_get_indev(e) == NULL) return; 

        settings.display.timeout_ms = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e)) * 1000;
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

// Data mining
static void collectPageValues()
{
    // ---- COMMON ----
    temperatureUnit = (settings.system.unitSystem == UnitSystem::METRIC) ? "°C" : "°F";

    g_last.current.language = settings.display.language;
    g_last.current.wifiConnected = vars.network.connected;

    if (g_last.current.wifiConnected)
    {
        wifi_ip_text = WiFi.localIP().toString().c_str();
    }    
    else
    {
        wifi_ip_text = "";
    }    

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
                    Log.serrorln(FPSTR(L_DISPLAY), F("Esp-now: Heartbeat sending error: %d!"), result);
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
                    Log.serrorln(FPSTR(L_DISPLAY), F("Esp-now: Data sending error: %d!"), result);
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
        temperatureUnit = (settings.system.unitSystem == UnitSystem::METRIC) ? "°C" : "°F";

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