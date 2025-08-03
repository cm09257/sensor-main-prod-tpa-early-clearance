#ifndef SETTINGS_H
#define SETTINGS_H
/**
 * @file settings.h
 * @brief Definitionen für Geräteeinstellungen, Kommando-Codes und Funkprotokoll.
 *
 * Diese Header-Datei enthält Konstanten und die Struktur `settings_t` für die persistente
 * Gerätekonfiguration, sowie IDs für Funkprotokollelemente laut DFP/SRS.
 */

#include <stdint.h>
#include "types.h"
#include "stm8s.h"

//////////// Debug Message Active Flags for individual files
#define DEBUG_MODE_TEST 1
#define DEBUG_MODE_WAIT_FOR_ACTIVATION 1
#define DEBUG_MODE_DATA_TRANSFER 1
#define DEBUG_MODE_HI_TEMP 1
#define DEBUG_MODE_OPERATIONAL 1
#define DEBUG_MODE_PRE_HI_TEMP 1
// #define DEBUG_STORAGE_C 1
#define DEBUG_MAIN_C 1
#define DEBUG_STATE_MACHINE_C 1

////////// General Configuration for All Configurations (i.e., debug & release)
#define DEVICE_ID_LSB 0x10
#define DEVICE_ID_MSB 0x00
#define DEVICE_OFFSET_HZ_23_DEG   0  // Freq offset @23deg in Hz

////////// Possible Build Configurations - choose either one or the other!
#define RELEASE_CONFIGURATION 1
//#define DEBUG_CONFIGURATION 1
// #define ITS_TOO_HOT 1 // use if outside temps are too hi to use finger-activation of tmp126

///////// RFM69 library size: tiny or full: Please select either one
#define RFM69_LIBRARY_TINY 1
//#define RFM69_LIBRARY_FULL 1

////////// Debug Configuration
#if defined(DEBUG_CONFIGURATION)
//// MODE_HI_TEMPERATURE
#define DEFAULT_COOL_DOWN_THRESHOLD 22.0f   // Tmp<DEFAULT_COOL_DOWN_THRESHOLD: --> MODE_OPERATIONAL
#define DEFAULT_HI_TMP_MEAS_INTERVAL_5MIN 2 // 2 ==> 10min  --> Global Debug flag overrides to 1min intervals!!!!

//// MODE_PRE_HI_TEMPERATURE
#define PRE_HIGH_TEMP_THRESHOLD_C 27.5f // Tmp>PRE_HIGH_TEMP_THRESHOLD: --> MODE_HI_TEMPERATURE

//// MODE_OPERATIONAL
#define DEFAULT_SEND_MODE 0 //////////////////////// 0 = periodic, 1 = fixed time
#define DEFAULT_SEND_INTERVAL_5MIN 2 /////////////// 2 = every 10min
#define DEFAULT_SEND_FIXED_HOUR 12
#define DEFAULT_SEND_FIXED_MINUTE 0
#define DEFAULT_SEND_TIME_WINDOW_ACTIVE 0            ///< 0 = kein Zeitfenster für Funken, 1 = Zeitfenster für funk ist aktiv
#define DEFAULT_SEND_TIME_WINDOW_FROM_HOUR 5         ///< Uhrzeit (nur Stunden) am Tag, ab der Funken erlaubt ist (send_time_window_active)
#define DEFAULT_SEND_TIME_WINDOW_UNTIL_HOUR 19       ///< Uhrzeit (nur Stunden) am Tag, bis zu der Funken erlaubt istF
#define DEFAULT_MEAS_MODE 0 //////////////////////// 0 = periodic, 1 = fixed time
#define DEFAULT_MEAS_INTERVAL_5MIN 1 /////////////// 1 = every 5min
#define DEFAULT_MEAS_FIXED_HOUR 10
#define DEFAULT_MEAS_FIXED_MINUTE 0

//// MODE_WAIT_FOR_ACTIVATION
#define MAX_ACTIVATION_PING_SEND_RETRIES 3
#define WAIT_FOR_ACT_ACK_TIMEOUT 1000
#define WAIT_FOR_ACT_CMD_TIMEOUT 200
#define DELAY_BEFORE_RETRY 100
#define WAIT_FOR_ACT_CMD_TIMEOUT_LOOP 100

//// MODE_DATA_TRANSFER
#define MAX_DT_XFER_PING_SEND_RETRIES 5
#define DT_XFER_PING_DELAY_BEFORE_RETRY 100
#define DT_XFER_ACK_TIMEOUT 1000
#define DT_XFER_CMD_TIMEOUT 200
#define TIMEOUT_DT_XFER_WAIT_FOR_CMD 100
#define DT_XFER_MAX_DT_PACKET_SEND_RETRIES 5

#endif

//////// Configuration for TPA Early Clearance Variant
#if defined(RELEASE_CONFIGURATION)
//// MODE_HI_TEMPERATURE
#define DEFAULT_COOL_DOWN_THRESHOLD 70.0f      // Tmp<DEFAULT_COOL_DOWN_THRESHOLD: --> MODE_OPERATIONAL
#define DEFAULT_HI_TMP_MEAS_INTERVAL_5MIN 1    // 1 ==> 5min  

//// MODE_PRE_HI_TEMPERATURE
#define PRE_HIGH_TEMP_THRESHOLD_C 75.0f        // Tmp>PRE_HIGH_TEMP_THRESHOLD: --> MODE_HI_TEMPERATURE

//// MODE_OPERATIONAL
#define DEFAULT_SEND_MODE 0                          /// 0 = periodic
#define DEFAULT_SEND_INTERVAL_5MIN 6                 /// 6 = 30min send intervals
#define DEFAULT_SEND_FIXED_HOUR 12                   /// not relevant, only for fixed-time alert
#define DEFAULT_SEND_FIXED_MINUTE 0                  /// not relevant, only for fixed-time alert
#define DEFAULT_SEND_TIME_WINDOW_ACTIVE 0            ///< 0 = no time window for send
#define DEFAULT_SEND_TIME_WINDOW_FROM_HOUR 5         ///< not relevant, no time window in this configuration
#define DEFAULT_SEND_TIME_WINDOW_UNTIL_HOUR 19       ///< not relevant, no time window in this configuration
#define DEFAULT_MEAS_MODE 0                          /// 0 = periodic
#define DEFAULT_MEAS_INTERVAL_5MIN 2                 /// 2 = 10min intervals
#define DEFAULT_MEAS_FIXED_HOUR 10                   /// not relevant, only for fixed-time alert
#define DEFAULT_MEAS_FIXED_MINUTE 0                  /// not relevant, only for fixed-time alert

//// MODE_WAIT_FOR_ACTIVATION
#define MAX_ACTIVATION_PING_SEND_RETRIES 3
#define WAIT_FOR_ACT_ACK_TIMEOUT 1000
#define WAIT_FOR_ACT_CMD_TIMEOUT 200
#define DELAY_BEFORE_RETRY 100
#define WAIT_FOR_ACT_CMD_TIMEOUT_LOOP 100

//// MODE_DATA_TRANSFER
#define MAX_DT_XFER_PING_SEND_RETRIES 5
#define DT_XFER_PING_DELAY_BEFORE_RETRY 100
#define DT_XFER_ACK_TIMEOUT 1000
#define DT_XFER_CMD_TIMEOUT 200
#define TIMEOUT_DT_XFER_WAIT_FOR_CMD 100
#define DT_XFER_MAX_DT_PACKET_SEND_RETRIES 5
#endif

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
    uint8_t send_time_window_active;             ///< 0 = kein Zeitfenster für Funken, 1 = Zeitfenster für funk ist aktiv
    uint8_t send_time_window_from_hour;          ///< Uhrzeit (nur Stunden) am Tag, ab der Funken erlaubt ist (send_time_window_active)
    uint8_t send_time_window_until_hour;         ///< Uhrzeit (nur Stunden) am Tag, bis zu der Funken erlaubt ist
    uint8_t meas_mode;                           ///< 0 = periodisch, 1 = feste Uhrzeit
    uint8_t meas_interval_5min;                  ///< nur bei meas_mode=0: Intervall (in 5-min Schritten)
    uint8_t meas_fixed_hour;                     ///< nur bei meas_mode=1: Stunde (0–23)
    uint8_t meas_fixed_minute;                   ///< nur bei meas_mode=1: Minute (0–59)
    float cool_down_threshold;                   ///< Temperatur-Schwelle
    uint8_t device_id_msb;                       ///< Eindeutige ID
    uint8_t device_id_lsb;                       ///< Eindeutige ID
    int32_t offset_hz;                           ///< RF-Freq. Offset @23°C
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


#endif // SETTINGS_H
