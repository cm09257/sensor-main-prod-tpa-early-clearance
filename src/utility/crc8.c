#include <stdint.h>

// CRC-8-ATM (auch bekannt als CRC-8)
// Polynom: x^8 + x^2 + x + 1 (0x07), Init: 0x00
uint8_t crc8_calc(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}
