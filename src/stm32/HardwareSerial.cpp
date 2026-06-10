#include "HardwareSerial.h"
#include "driver/uart.h"

namespace stm32 {

void HardwareSerial::begin(uint32_t baudRate, uint32_t tx, uint32_t rx) {
    uart_config_t uart_config = {
        .baud_rate = (int)baudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };
    
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, rx, tx, -1, -1);
}

void HardwareSerial::send(uint8_t *data, int len) {
    uart_write_bytes(UART_NUM_1, data, len);
}

uint8_t HardwareSerial::read() {
    int len = uart_read_bytes(UART_NUM_1, _data, 1, 0);
    return len == 1 ? _data[0] : 255;
}

bool HardwareSerial::available() {
    size_t size;
    uart_get_buffered_data_len(UART_NUM_1, &size);
    return size > 0;
}

}  // namespace stm32