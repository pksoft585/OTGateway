#include <LeanTask.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "display/espnow_data.h"

void addPeer(const uint8_t *mac, uint8_t channel)
{
    if (esp_now_is_peer_exist(mac)) return;

    esp_now_peer_info_t peerInfo = {}; 
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = channel;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Esp-now: Add peer error!");
    }
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
    if (len < 1) return;
    uint8_t type = incomingData[0];
    uint32_t current_time = millis();

/*
    Serial.printf("Esp-now: Raw data - length: %d, type: %d\n", len, type);
*/
    switch(type)
    {
        case PAIR_RESPONSE:
            if (len == sizeof(pairingData))
            {
                memcpy(&espnow_pairing, incomingData, sizeof(pairingData));
                memcpy(thermostatMac, espnow_pairing.macAddr, 6);
                thermostatChannel = espnow_pairing.channel;

                Serial.printf("Esp-now: Thermostat MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                            thermostatMac[0], thermostatMac[1], thermostatMac[2], 
                            thermostatMac[3], thermostatMac[4], thermostatMac[5]);
                Serial.printf("Esp-now: Thermostat channel: %d\n", thermostatChannel);

                esp_now_del_peer(broadcastMac);
                addPeer(thermostatMac, thermostatChannel);
                
                espnow.paired = true;
                espnow.last_data = current_time;
                espnow.lastPairAttempt = current_time; 
                Serial.println("Esp-now: Thermostat paired");
            }
            break;

        case DATA_TERM:
            if (len == sizeof(thermostatValues))
            {
                memcpy(&espnow_therm, incomingData, sizeof(thermostatValues));
                Serial.println("Esp-now: Thermostat data received");
                espnow.last_data = current_time;
                espnow.received = true;
            }
            break;

        case HEARTBEAT:
            if (len == sizeof(heartbeatData))
            {
//                Serial.println("Esp-now: Heartbeat received");
                espnow.last_data = current_time;
            }
            break;

    }
}

bool init_esp_now()
{
    WiFi.mode(WIFI_STA);

    esp_wifi_set_country_code("EU", true); 
/*
    wifi_country_t country = {.cc = "EU", .schan = 1, .nchan = 13, .max_tx_power = 20, .policy = WIFI_COUNTRY_POLICY_AUTO};
    esp_wifi_set_country(&country);
*/    
    
    while (!WiFi.STA.started())
    {
        Serial.println("Esp-now: Waiting for Wi-Fi start...");
        delay(50);
    }
    Serial.println("Esp-now: Wi-Fi ready");

    WiFi.setChannel(thermostatChannel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("Esp-now: Wi-Fi set to channel: %d\n", thermostatChannel);

    WiFi.macAddress(displayMac);

    Serial.printf("Esp-now: Display MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                displayMac[0], displayMac[1], displayMac[2], 
                displayMac[3], displayMac[4], displayMac[5]);
    
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Esp-now: Initialization failed!");
        return false;
    }

    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    addPeer(broadcastMac, thermostatChannel);

    return true;
}

class EspnowTask : public LeanTask
{
public:
  EspnowTask(bool _enabled = false, unsigned long _interval = 0) : LeanTask(_enabled, _interval) {}

protected:

#if defined(ARDUINO_ARCH_ESP32)
    const char *getTaskName() override
    {
        return "EspnowTask";
    }

    BaseType_t getTaskCore() override 
    {
        return 1;
    }

    int getTaskPriority() override
    {
        return 3;
    }
#endif

private:
    void setup() override
    {
        if (thermostatChannel > 1) current_scan_channel = thermostatChannel - 1;
        espnow.initialized = init_esp_now();
    }

    void loop() override
    {
        uint32_t current_time = millis();

        if (!espnow.paired)
        {
            if (espnow.lastPairAttempt == 0)
            {
                espnow.lastPairAttempt = current_time;
                vTaskDelay(pdMS_TO_TICKS(500)); 
                return;
            }

            if (current_time - espnow.lastPairAttempt >= CHANNEL_SWITCH_DELAY)
            {
                espnow.lastPairAttempt = current_time;

                current_scan_channel++;
                if (current_scan_channel > 13) current_scan_channel = 1;

                WiFi.setChannel(current_scan_channel, WIFI_SECOND_CHAN_NONE);
                Serial.printf("Esp-now: Wi-Fi set to channel: %d\n", current_scan_channel);

                vTaskDelay(pdMS_TO_TICKS(100)); 

                esp_now_del_peer(broadcastMac);
                addPeer(broadcastMac, current_scan_channel);

                espnow_pairing.msgType = PAIR_REQUEST;
                espnow_pairing.id = 1;
                memcpy(espnow_pairing.macAddr, displayMac, 6);
                espnow_pairing.channel = current_scan_channel;

//                Serial.println("Esp-now: Pairing data sending...");
                esp_err_t result = esp_now_send(broadcastMac, (uint8_t *) &espnow_pairing, sizeof(pairingData));
                if (result == ESP_OK)
                {
                    Serial.println("Esp-now: Pairing data sent");
                }
                else
                {
                    Serial.printf("Esp-now: Pairing data sending error: %d!\n", result);
                }
            }    
        }
        else
        {
            if (espnow.paired && (current_time - espnow.last_data > espnow.timeout_ms))
            {
                Serial.println("Esp-now: Connection lost");
                espnow.paired = false;
                esp_now_del_peer(thermostatMac);
                current_scan_channel = thermostatChannel - 1;
            }

            if (espnow.to_sent)
            {
//                Serial.println("Esp-now: Display data sending...");
                esp_err_t result = esp_now_send(thermostatMac, (uint8_t *)&espnow_disp, sizeof(displayValues));
                if (result == ESP_OK)
                {
                    Serial.println("Esp-now: Display data sent");
                }
                else
                {
                    Serial.printf("Esp-now: Display data sending error: %d!\n", result);
                }

                espnow.to_sent = false;
            }
        }
    }    
};
