#pragma once

#include <Arduino.h>
#include "dt_pages.h"

#pragma pack(push, 1)

enum MessageType : uint8_t { 
    PAIR_REQUEST = 1, 
    PAIR_RESPONSE,
    DATA_TERM,
    DATA_DISP,
    HEARTBEAT 
};

struct pValues {
    int arcValue = 500;
    int arcMinValue = 0;
    int arcMaxValue = 1000;
    int currentValue = 0;
    bool enabled = false;
    bool turbo = false;
    bool active = false;
};

struct cValues {
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool openthermConnected = false;
    bool flame = false;
    uint32_t unixTime = 0;
    Language language = Language::EN;
    uint8_t unitSys = 0;
    uint8_t wifiIp[4] = {0, 0, 0, 0};
};

struct thermostatValues {
    uint8_t msgType = DATA_TERM;
    uint8_t id = 0;
    cValues common;
    pValues pages[static_cast<uint8_t>(DisplayPage::COUNT)];
};

struct displayValues {
    uint8_t msgType = DATA_DISP;
    uint8_t id = 1;
    int heatingTarget = 0;
    int dhwTarget = 0;
    int backlightTarget = 0;
    bool heatingEnabled = false;
    bool heatingTurbo = false;
    bool dhwEnabled = false;
    Language language = Language::EN;
    uint32_t dispTimeout = 30000;
};

struct settingsValues {
    uint8_t unitSys = 0;
    uint8_t wifiIp[4] = {0, 0, 0, 0};
};

struct pairingData {
    uint8_t msgType;
    uint8_t id;
    uint8_t macAddr[6];
    uint8_t channel;
};

struct heartbeatData {
    uint8_t msgType = HEARTBEAT;
    uint8_t id = 0;
};

#pragma pack(pop)

static thermostatValues espnow_therm;
static displayValues espnow_disp;
static settingsValues espnow_settings;
static pairingData espnow_pairing;
static heartbeatData espnow_heartbeat;

struct
{
    bool initialized = false;
    bool paired = false;
    bool to_sent = false;
    bool received = false;
    uint32_t last_data = 0;
    uint32_t timeout_ms = 15000; 
    uint32_t lastPairAttempt = 0;
    uint32_t hearbeatTime = 10000; 
} espnow;

uint8_t displayMac[6];
uint8_t thermostatMac[6];
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t displayChannel = 6;
uint8_t thermostatChannel = 6;

uint8_t current_scan_channel = 1; 
const uint16_t CHANNEL_SWITCH_DELAY = 1000;

static void defaultPageValues()
{
    espnow_therm.pages[0].arcMinValue = 100;
    espnow_therm.pages[0].arcMaxValue = 500;
    espnow_therm.pages[0].arcValue = 100;
    espnow_therm.pages[1].arcMinValue = 30;
    espnow_therm.pages[1].arcMaxValue = 90;
    espnow_therm.pages[1].arcValue = 30;
    espnow_therm.pages[2].arcMinValue = 0;
    espnow_therm.pages[2].arcMaxValue = 100;
    espnow_therm.pages[2].arcValue = 85;
    espnow_therm.pages[2].currentValue = 30;
}
