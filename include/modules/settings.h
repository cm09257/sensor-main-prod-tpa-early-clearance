#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include "common/types.h"
#include "stm8s.h"

/**
 * @file settings.h
 * @brief Definitionen für Geräteeinstellungen, Kommando-Codes und Funkprotokoll.
 *
 * Diese Header-Datei enthält Konstanten und die Struktur `settings_t` für die persistente
 * Gerätekonfiguration, sowie IDs für Funkprotokollelemente laut DFP/SRS.
 */

// === Feature-Konfiguration ===

/// Aktiviert zyklische Messung im MODE_PRE_HIGH_TEMP
//#define PRE_HIGH_TEMP_MEASURE 1


// === Defaultwerte ===

/// Schwelle für das Beenden des HIGH_TEMPERATURE-Modus (°C)
#define DEFAULT_COOL_DOWN_THRESHOLD 50.0f   


/// Schwelle für den Übergang in den HIGH_TEMPERATURE-Modus (°C)
#define PRE_HIGH_TEMP_THRESHOLD_C 100.0f


// === Wiederholstrategien ===

/// Maximale Anzahl Ping-Wiederholungen bei fehlender Antwort
#define MAX_PING_RETRIES     5

/// Maximale Anzahl Wiederholungen bei NACK (z. B. CRC-Fehler)
#define MAX_NACK_RETRIES     5


// === Uplink: Chunking-Konfiguration ===

/// Max. Anzahl Records pro Funkpaket (limitierend: RFM69 → <64 Byte)
#define MAX_RECORDS_PER_PACKET 10
// → ergibt bei 5 Byte/Record + 8 Byte Overhead genau 58 Byte total


// === Header-Codes laut DFP/SRS ===

/// Header für Aktivierungs-Pings
#define RADIO_PING_HEADER_ACTIVATION     0xA0

/// Header für Transfer-Pings
#define RADIO_PING_HEADER_DATA_TRANSFER  0xA1

/// Header für Uplink-Datenpakete
#define RADIO_DATA_PACKET_HEADER         0xA2

/// Header für Downlink-Kommandos
#define RADIO_DOWNLINK_HEADER            0xB0  


// === Downlink-Befehle (DFP-3) ===

#define CMD_SET_MEASUREMENT_INTERVAL      0x01  // Intervall der Temperaturmessung setzen
#define CMD_SET_SEND_INTERVAL             0x02  // Sendeintervall (Faktor) konfigurieren
#define CMD_SET_HIGH_TEMP_THRESHOLD       0x03  // Grenzwert für HIGH_TEMPERATURE-Modus
#define CMD_SET_RTC_OFFSET                0x04  // Zeitverschiebung / Referenzjustierung
#define CMD_ERASE_FLASH                   0x05  // Alle gespeicherten Datensätze löschen
#define CMD_SET_TRANSFER_STRATEGY         0x06  // 0 = alle, 1 = nur neue Datensätze
#define CMD_SET_ACTIVATION_MODE           0x07  // (derzeit nicht zur Laufzeit erlaubt)
#define CMD_SOFT_RESET                    0x08  // Gerätesoftware neu starten
#define CMD_ACTIVATION                    0x09  // Geräteaktivierung (z. B. nach Erstinstallation)


// === Funkbestätigungscodes ===

#define RADIO_ACK_CODE  0xAA
#define RADIO_NACK_CODE 0x55


// === Struktur der Gerätekonfiguration ===

/**
 * @brief Persistente Geräteeinstellungen, gespeichert im EEPROM
 */
typedef struct {
    uint8_t measurement_interval_5min;            ///< Intervall zwischen Messungen (in 5-Min-Schritten)
    uint8_t high_temp_measurement_interval_5min;  ///< Intervall im HIGH_TEMPERATURE-Modus
    uint8_t send_interval_factor;                 ///< Messzyklen pro Uplink (z. B. 3 = jede 3. Messung)
    uint8_t transfer_mode;                        ///< 0 = alle Daten, 1 = nur neue Datensätze (⚠️ Phase 2)
    uint32_t device_id;                           ///< Eindeutige 32-Bit Geräte-ID
    float cool_down_threshold;                    ///< Temperatur-Schwelle zum Beenden von HIGH_TEMPERATURE
} settings_t;


// === Zugriff auf Einstellungen ===

/**
 * @brief Gibt aktuellen Settings-Zeiger zurück (readonly)
 */
const settings_t* settings_get(void);

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
