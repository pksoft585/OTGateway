/*
 * OpenTherm STM32 protocol implementation for DIYLESS Thermostat 3
 * Adapted from ESPHome OpenTherm component
 * 
 * This version communicates with STM32 via UART (200kbaud) using custom protocol
 * Original code based on: https://github.com/jpraus/arduino-opentherm
 */

#pragma once

#include <Arduino.h>
#include <cstdint>
#include "HardwareSerial.h"
#include "Stm32AppProtocol.h"

namespace stm32 {

/**
 * OpenTherm data packet structure
 */
struct OpenthermData {
  uint8_t type;
  uint8_t id;
  uint8_t valueHB;
  uint8_t valueLB;

  OpenthermData() : type(0), id(0), valueHB(0), valueLB(0) {}

  float f88();
  void f88(float value);

  uint16_t u16();
  void u16(uint16_t value);

  int16_t s16();
  void s16(int16_t value);
};

/**
 * OpenTherm message types (from specification)
 */
enum MessageType {
  READ_DATA = 0,
  WRITE_DATA = 1,
  INVALID_DATA = 2,
  READ_ACK = 4,
  WRITE_ACK = 5,
  DATA_INVALID = 6,
  UNKNOWN_DATAID = 7
};

/**
 * OpenTherm message IDs (from specification)
 */
enum MessageId {
  STATUS = 0,
  CH_SETPOINT = 1,
  CONTROLLER_CONFIG = 2,
  DEVICE_CONFIG = 3,
  COMMAND_CODE = 4,
  FAULT_FLAGS = 5,
  REMOTE = 6,
  COOLING_CONTROL = 7,
  CH2_SETPOINT = 8,
  CH_SETPOINT_OVERRIDE = 9,
  TSP_COUNT = 10,
  TSP_COMMAND = 11,
  FHB_SIZE = 12,
  FHB_COMMAND = 13,
  MAX_MODULATION_LEVEL = 14,
  MAX_BOILER_CAPACITY = 15,
  ROOM_SETPOINT = 16,
  MODULATION_LEVEL = 17,
  CH_WATER_PRESSURE = 18,
  DHW_FLOW_RATE = 19,
  DAY_TIME = 20,
  DATE = 21,
  YEAR = 22,
  ROOM_SETPOINT_CH2 = 23,
  ROOM_TEMP = 24,
  FEED_TEMP = 25,
  DHW_TEMP = 26,
  OUTSIDE_TEMP = 27,
  RETURN_WATER_TEMP = 28,
  SOLAR_STORE_TEMP = 29,
  SOLAR_COLLECT_TEMP = 30,
  FEED_TEMP_CH2 = 31,
  DHW2_TEMP = 32,
  EXHAUST_TEMP = 33,
  FAN_SPEED = 35,
  FLAME_CURRENT = 36,
  ROOM_TEMP_CH2 = 37,
  REL_HUMIDITY = 38,
  DHW_BOUNDS = 48,
  CH_BOUNDS = 49,
  OTC_CURVE_BOUNDS = 50,
  DHW_SETPOINT = 56,
  MAX_CH_SETPOINT = 57,
  OTC_CURVE_RATIO = 58,
  HVAC_STATUS = 70,
  REL_VENT_SETPOINT = 71,
  DEVICE_VENT = 74,
  HVAC_VER_ID = 75,
  REL_VENTILATION = 77,
  REL_HUMID_EXHAUST = 78,
  EXHAUST_CO2 = 79,
  SUPPLY_INLET_TEMP = 80,
  SUPPLY_OUTLET_TEMP = 81,
  EXHAUST_INLET_TEMP = 82,
  EXHAUST_OUTLET_TEMP = 83,
  EXHAUST_FAN_SPEED = 84,
  SUPPLY_FAN_SPEED = 85,
  REMOTE_VENTILATION_PARAM = 86,
  NOM_REL_VENTILATION = 87,
  HVAC_NUM_TSP = 88,
  HVAC_IDX_TSP = 89,
  HVAC_FHB_SIZE = 90,
  HVAC_FHB_IDX = 91,
  RF_SIGNAL = 98,
  DHW_MODE = 99,
  OVERRIDE_FUNC = 100,
  SOLAR_MODE_FLAGS = 101,
  SOLAR_ASF = 102,
  SOLAR_VERSION_ID = 103,
  SOLAR_PRODUCT_ID = 104,
  SOLAR_NUM_TSP = 105,
  SOLAR_IDX_TSP = 106,
  SOLAR_FHB_SIZE = 107,
  SOLAR_FHB_IDX = 108,
  SOLAR_STARTS = 109,
  SOLAR_HOURS = 110,
  SOLAR_ENERGY = 111,
  SOLAR_TOTAL_ENERGY = 112,
  FAILED_BURNER_STARTS = 113,
  BURNER_FLAME_LOW = 114,
  OEM_DIAGNOSTIC = 115,
  BURNER_STARTS = 116,
  CH_PUMP_STARTS = 117,
  DHW_PUMP_STARTS = 118,
  DHW_BURNER_STARTS = 119,
  BURNER_HOURS = 120,
  CH_PUMP_HOURS = 121,
  DHW_PUMP_HOURS = 122,
  DHW_BURNER_HOURS = 123,
  OT_VERSION_CONTROLLER = 124,
  OT_VERSION_DEVICE = 125,
  VERSION_CONTROLLER = 126,
  VERSION_DEVICE = 127
};

/**
 * OpenTherm STM32 Gateway
 * Communicates with STM32 via UART at 200kbaud
 */
class OpenTherm {
public:
  OpenTherm() {}
  
  /**
   * Initialize OpenTherm communication
   * Must be called AFTER STM32 boot sequence
   */
  bool begin(uint8_t rxPin, uint8_t txPin);
  
  /**
   * Process incoming data from STM32
   * Call periodically in main loop
   */
  void loop();
  
  /**
   * Send OpenTherm request to STM32
   * Blocks until response or timeout
   */
  unsigned long sendRequest(unsigned long request, uint32_t timeoutMs = 1000);
  
  /**
   * Get last response payload
   */
  unsigned long getLastResponse() const {
    return lastResponse;
  }
  
  /**
   * Check if last response was valid
   */
  bool hasValidResponse() const {
    return lastResponseValid;
  }

private:
  HardwareSerial uart;
  ProtocolSerializer m_Serializer;
  uint8_t m_TxBuffer[ProtocolSerializer::BUFFER_SIZE];
  
  uint32_t lastResponse = 0;
  bool lastResponseValid = false;
  
  void onOtCommandResponse(const OtCommandResponse *resp);
};

}  // namespace stm32
