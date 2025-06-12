#include "config/config.h"
#include "modules/settings.h"
#include "modules/storage.h"
#include <string.h>
#include "stm8s.h"
#include "periphery/uart.h"
#include "utility/debug.h"
#include "utility/delay.h"

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

    current_settings.high_temp_measurement_interval_5min = 2;           ///< Intervall im HIGH_TEMPERATURE-Modus
    current_settings.transfer_mode = 0;                                 ///< 0 = alle Daten, 1 = nur neue Datensätze
    current_settings.flags = 0x00;                                      ///< z. B. Bit 0 = Flash initialized
    current_settings.flash_record_count;                                ///< Anzahl Datensätze im externen Flash
    current_settings.send_mode = 0;                                     ///< 0 = periodisch, 1 = feste Uhrzeit
    current_settings.send_interval_5min = 12;                           ///< nur bei send_mode=0: Intervall (in 5-min Schritten)
    current_settings.send_fixed_hour = 12;                              ///< nur bei send_mode=1: Stunde (0–23)
    current_settings.send_fixed_minute = 0;                             ///< nur bei send_mode=1: Minute (0–59)
    current_settings.meas_mode = 0;                                     ///< 0 = periodisch, 1 = feste Uhrzeit
    current_settings.meas_interval_5min = 2;                            ///< nur bei meas_mode=0: Intervall (in 5-min Schritten)
    current_settings.meas_fixed_hour = 10;                              ///< nur bei meas_mode=1: Stunde (0–23)
    current_settings.meas_fixed_minute = 0;                             ///< nur bei meas_mode=1: Minute (0–59)
    current_settings.cool_down_threshold = DEFAULT_COOL_DOWN_THRESHOLD; ///< Temperatur-Schwelle
    current_settings.device_id = 0xFFFFFFFFF;                           ///< Eindeutige ID
    current_settings.flash_record_count = 0;
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

        // Plausibilitätscheck für neue Felder
        if (current_settings.meas_mode > 1 ||
            current_settings.send_mode > 1 ||
            (current_settings.meas_mode == 0 && current_settings.meas_interval_5min == 0) ||
            (current_settings.send_mode == 0 && current_settings.send_interval_5min == 0) ||
            current_settings.send_fixed_hour > 23 ||
            current_settings.send_fixed_minute > 59 ||
            current_settings.meas_fixed_hour > 23 ||
            current_settings.meas_fixed_minute > 59 ||
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

void settings_set_cool_down_threshold(float threshold)
{
    current_settings.cool_down_threshold = threshold;
    delay(5);
    settings_save();
    delay(5);
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
