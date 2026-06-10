#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0

#include <Arduino.h>
#include <WiFi.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include <ESP32Scheduler.h>
#else
  #error Wrong board. Supported boards: esp32
#endif

#include <Task.h>
#include <LeanTask.h>
#include "DisplayTaskLocal.h"
#include "EspnowTaskLocal.h"

DisplayTask* tDisplay = nullptr;
EspnowTask* tEspnow = nullptr;

void setup() {
  Serial.begin(115200);
  delay(500); 
  Serial.println("\n--- ESP32-S3 initializing ---");

  // Display init
  Serial.println("Display init.");
  auto display_init_result = display_init();

  //
  // Make tasks
  if (display_init_result == DisplayInitResult::OK) {
    Serial.println("Display Task ready to start.");
    tDisplay = new DisplayTask(true, DISPLAYTASK_INTERVAL);
    Scheduler.start(tDisplay);
    Serial.println("Display Task started.");

    Serial.println("Esp-Now Task ready to start.");
    tEspnow = new EspnowTask(true, 50);
    Scheduler.start(tEspnow);
    Serial.println("Esp-Now Task started.");

    Scheduler.begin();
    Serial.println("Scheduler begin.");
  } else {
    Serial.print("Display initialization failed, reason = ");
    Serial.println(displayInitResultToString(display_init_result));
  }
  
  Serial.println("--- ESP32-S3 initialized ---\n");
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  vTaskDelete(NULL);
#endif
}
