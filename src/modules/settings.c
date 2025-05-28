#include "config/config.h"
#include "modules/settings.h"
#include "modules/storage.h"
#include <string.h>
#include "stm8s.h"
#include "periphery/uart.h"
#include "utility/debug.h"

#define SETTINGS_ADDR 0x30 // Startadresse im EEPROM
#define SETTINGS_CRC_ADDR (SETTINGS_ADDR + sizeof(settings_t))

#define MODE_ADDR 0x20
#define MODE_CRC_ADDR (MODE_ADDR + sizeof(mode_t))

static settings_t current_settings;

// CRC8 (einfach, z. B. XOR über alle Bytes)
static uint8_t calc_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i)
        crc ^= data[i];
    return crc;
}

const settings_t *settings_get(void)
{
    return &current_settings;
}

void settings_set_default(void)
{
    DebugLn("[settings] Setze Default-Werte...");

    current_settings.measurement_interval_5min = 12;          // 60 Minuten Messintervall
    current_settings.high_temp_measurement_interval_5min = 2; // 10 Minuten bei HIGH_TEMPERATURE
    current_settings.send_interval_factor = 4;                // alle 4×Messung = 4×60min = 4h
    current_settings.transfer_mode = 0;                       // 0 = alle Daten übertragen
    current_settings.device_id = 0xFFFFFFFF;                  // ungültige ID
    current_settings.cool_down_threshold = DEFAULT_COOL_DOWN_THRESHOLD;

    // DebugUVal("  Intervall: ", current_settings.measurement_interval_5min, " x5min");
    // DebugUVal("  HighTemp-Intervall: ", current_settings.high_temp_measurement_interval_5min, " x5min");
    // DebugUVal("  Funk-Intervall-Faktor: ", current_settings.send_interval_factor, "");
    // DebugUVal("  Device ID: ", current_settings.device_id, "");
    // DebugIVal("  CoolDown-Schwelle: ", (int)(current_settings.cool_down_threshold * 10), " x0.1°C");
}

void settings_load(void)
{
    uint8_t raw[sizeof(settings_t)];
    uint8_t crc_stored = 0;

    storage_read_eeprom(SETTINGS_ADDR, raw, sizeof(settings_t));
    storage_read_eeprom(SETTINGS_CRC_ADDR, &crc_stored, 1);

    uint8_t crc_calc = calc_crc8(raw, sizeof(settings_t));

    if (crc_calc == crc_stored)
    {
        memcpy(&current_settings, raw, sizeof(settings_t));

        if (current_settings.measurement_interval_5min == 0 ||
            current_settings.send_interval_factor == 0 ||
            current_settings.high_temp_measurement_interval_5min == 0)
        {
            settings_set_default();
            settings_save();
        }
    }
    else
    {
        settings_set_default();
        settings_save();
    }
}

void settings_save(void)
{
    uint8_t raw[sizeof(settings_t)];
    memcpy(raw, &current_settings, sizeof(settings_t));

    uint8_t crc = calc_crc8(raw, sizeof(settings_t));

    // EEPROM: zuerst die eigentlichen Daten speichern
    storage_write_eeprom(SETTINGS_ADDR, raw, sizeof(settings_t));

    // danach die CRC speichern
    storage_write_eeprom(SETTINGS_CRC_ADDR, &crc, 1);
}
