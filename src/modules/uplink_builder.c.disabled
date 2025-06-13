// uplink_builder.c – erstellt Uplink-Datenpakete nach DFP-2
#include "modules/uplink_builder.h"
#include "modules/settings.h"
#include "utility/crc8.h"
#include "config/config.h"
#include <string.h>

// CRC-4 über 20 Bit Zeitstempel (z. B. über Bytes 0, 1, 2[7:4])
static uint8_t crc4_over_timestamp(uint32_t ts_5min) {
    // CRC-4-ITU: Polynom 0x03, Init 0x00
    uint8_t crc = 0;
    crc ^= (ts_5min >> 0) & 0xFF;
    crc ^= (ts_5min >> 8) & 0xFF;
    crc ^= (ts_5min >> 16) & 0x0F;
    for (uint8_t i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x03 : (crc << 1);
    return crc & 0x0F;
}

// Erstellt ein vollständiges Uplink-Paket und schreibt es in out_buf
// Liefert die finale Paketlänge zurück
uint8_t uplink_build_packet(const record_t* records, uint8_t count,
                            const uint8_t* device_id, uint8_t seq_nr,
                            uint8_t* out_buf) {
    uint8_t pos = 0;

    // Header für Datenpakete
    out_buf[pos++] = RADIO_DATA_PACKET_HEADER;  // 0xA2
    
    out_buf[pos++] = seq_nr;               // 1 Byte Sequenznummer
    memcpy(&out_buf[pos], device_id, 4);   // 4 Byte Geräte-ID
    pos += 4;
    out_buf[pos++] = count * 5;            // Payload-Länge in Bytes (5 Byte pro Record)

    // Einzelne Messdatensätze kodieren
    for (uint8_t i = 0; i < count; ++i) {
        const record_t* r = &records[i];
        uint32_t ts = r->timestamp;
        uint8_t crc4 = crc4_over_timestamp(ts);  // CRC-4 über 20 Bit Zeitstempel

        // Zeitstempel (20 Bit) + CRC-4 (4 Bit)
        out_buf[pos++] = (ts >> 0) & 0xFF;              // Bits 0–7
        out_buf[pos++] = (ts >> 8) & 0xFF;              // Bits 8–15
        out_buf[pos] = ((ts >> 16) & 0x0F) << 4;        // Bits 16–19 in Bits 4–7
        out_buf[pos++] |= (crc4 & 0x0F);                // CRC in Bits 0–3

        // Temperatur in Fixed-Point mit Offset –50.0 °C → 0
        int16_t temp_raw = (int16_t)((r->temperature + 50.0f) * 16.0f);

        out_buf[pos++] = ((temp_raw >> 4) & 0xFF);      // 8 MSB der Temperatur
        out_buf[pos++] = ((temp_raw & 0x0F) << 4)       // 4 LSB der Temperatur
                         | (r->flags & 0x0F);           // 4 Bit Flags
    }

    // CRC-8 über alle bisherigen Bytes berechnen
    uint8_t crc8 = crc8_calc(out_buf, pos);
    out_buf[pos++] = crc8;

    return pos;  // Gesamtlänge des Pakets
}