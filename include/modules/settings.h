#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include "types.h"
#include "stm8s.h"

//////////// Debug Message Active Flags for ind. files

#define DEBUG_MODE_TEST 1
#define DEBUG_MODE_WAIT_FOR_ACTIVATION 1
//#define DEBUG_MODE_DATA_TRANSFER 1
//#define DEBUG_MODE_HI_TEMP 1
//#define DEBUG_MODE_OPERATIONAL 1
//#define DEBUG_MODE_PRE_HI_TEMP 1
//#define DEBUG_STORAGE_C 1
#define DEBUG_MAIN_C 1
#define DEBUG_STATE_MACHINE_C 1

#define DEVICE_ID_LSB 0x00
#define DEVICE_ID_MSB 0xAA


/**
 * @file settings.h
 * @brief Definitionen für Geräteeinstellungen, Kommando-Codes und Funkprotokoll.
 *
 * Diese Header-Datei enthält Konstanten und die Struktur `settings_t` für die persistente
 * Gerätekonfiguration, sowie IDs für Funkprotokollelemente laut DFP/SRS.
 */

// === Feature-Konfiguration ===

/// Aktiviert zyklische Messung im MODE_PRE_HIGH_TEMP
// #define PRE_HIGH_TEMP_MEASURE 1

// === Defaultwerte ===

/// Schwelle für das Beenden des HIGH_TEMPERATURE-Modus (°C)
#define DEFAULT_COOL_DOWN_THRESHOLD 23.0f

/// Schwelle für den Übergang in den HIGH_TEMPERATURE-Modus (°C)
#define PRE_HIGH_TEMP_THRESHOLD_C 27.0f

// === Wiederholstrategien ===

/// Maximale Anzahl Ping-Wiederholungen bei fehlender Antwort
#define MAX_PING_RETRIES 5

/// Maximale Anzahl Wiederholungen bei NACK (z. B. CRC-Fehler)
#define MAX_NACK_RETRIES 5

// === Uplink: Chunking-Konfiguration ===

/// Max. Anzahl Records pro Funkpaket (limitierend: RFM69 → <64 Byte)
#define MAX_RECORDS_PER_PACKET 10
// → ergibt bei 5 Byte/Record + 8 Byte Overhead genau 58 Byte total

// === Header-Codes laut DFP/SRS ===

/// Header für Aktivierungs-Pings
#define RADIO_PING_HEADER_ACTIVATION 0xA0

/// Header für Transfer-Pings
#define RADIO_PING_HEADER_DATA_TRANSFER 0xA1

/// Header für Uplink-Datenpakete
#define RADIO_DATA_PACKET_HEADER 0xA2

/// Header für Downlink-Kommandos
#define RADIO_DOWNLINK_HEADER 0xB0

// === Downlink-Befehle (DFP-3) ===

#define CMD_SET_MEASUREMENT_INTERVAL 0x01 // Intervall der Temperaturmessung setzen
#define CMD_SET_SEND_INTERVAL 0x02        // Sendeintervall (Faktor) konfigurieren
#define CMD_SET_HIGH_TEMP_THRESHOLD 0x03  // Grenzwert für HIGH_TEMPERATURE-Modus
#define CMD_SET_RTC_OFFSET 0x04           // Zeitverschiebung / Referenzjustierung
#define CMD_ERASE_FLASH 0x05              // Alle gespeicherten Datensätze löschen
#define CMD_SET_TRANSFER_STRATEGY 0x06    // 0 = alle, 1 = nur neue Datensätze
#define CMD_SET_ACTIVATION_MODE 0x07      // (derzeit nicht zur Laufzeit erlaubt)
#define CMD_SOFT_RESET 0x08               // Gerätesoftware neu starten
#define CMD_ACTIVATION 0x09               // Geräteaktivierung (z. B. nach Erstinstallation)

// === Funkbestätigungscodes ===

#define RADIO_ACK_CODE 0xAA
#define RADIO_NACK_CODE 0x55

// === Flags ===
#define SETTINGS_FLAG_FLASH_ERASE_DONE (1 << 0)

// === Struktur der Gerätekonfiguration ===

/**
 * @brief Persistente Geräteeinstellungen, gespeichert im EEPROM
 */
typedef struct
{ 
    uint8_t high_temp_measurement_interval_5min; ///< Intervall im HIGH_TEMPERATURE-Modus
    uint8_t transfer_mode;                       ///< 0 = alle Daten, 1 = nur neue Datensätze
    uint8_t flags;                               ///< z. B. Bit 0 = Flash initialized
    uint8_t flash_record_count;                  ///< Anzahl Datensätze im externen Flash
    uint8_t send_mode;                           ///< 0 = periodisch, 1 = feste Uhrzeit
    uint8_t send_interval_5min;                  ///< nur bei send_mode=0: Intervall (in 5-min Schritten)
    uint8_t send_fixed_hour;                     ///< nur bei send_mode=1: Stunde (0–23)
    uint8_t send_fixed_minute;                   ///< nur bei send_mode=1: Minute (0–59)
    uint8_t meas_mode;                           ///< 0 = periodisch, 1 = feste Uhrzeit
    uint8_t meas_interval_5min;                  ///< nur bei meas_mode=0: Intervall (in 5-min Schritten)
    uint8_t meas_fixed_hour;                     ///< nur bei meas_mode=1: Stunde (0–23)
    uint8_t meas_fixed_minute;                   ///< nur bei meas_mode=1: Minute (0–59)
    float cool_down_threshold;                   ///< Temperatur-Schwelle
    uint8_t device_id_msb;                          ///< Eindeutige ID
    uint8_t device_id_lsb;                          ///< Eindeutige ID
} settings_t;

// === Zugriff auf Einstellungen ===

/**
 * @brief Gibt aktuellen Settings-Zeiger zurück (readonly)
 */
settings_t *settings_get(void);

/**
 * @brief Lädt Einstellungen aus EEPROM (mit CRC-Validierung)
 */
void settings_load(void);

/**
 * @brief Speichert aktuelle Einstellungen ins EEPROM
 */
void settings_save(void);

/**
 * @brief Setzt alle Einstellungen auf Default-Werte
 */
void settings_set_default(void);

void settings_set_cool_down_threshold(float threshold);

#endif // SETTINGS_H
