#pragma once

#include <Arduino.h>

namespace stm32 {

class HardwareSerial {
private:
    static constexpr int BUF_SIZE = 256;

    uint8_t _data[BUF_SIZE];
    
public:
    void begin(uint32_t baudRate, uint32_t tx, uint32_t rx);
    bool available();
    uint8_t read();
    void send(uint8_t *data, int len);
};

}  // namespace stm32