#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/**
 * @file types.h
 * @brief Zentrale Typdefinitionen für das Sensorprojekt
 *
 * Beinhaltet alle global verwendeten Datenstrukturen und Enumerationen,
 * die für Zustände, Ereignisse und Datensätze verwendet werden.
 */

/**
 * @typedef timestamp_t
 * @brief Zeitstempel in 5-Minuten-Schritten seit Referenzpunkt
 *
 * Die Zeitangabe basiert auf dem MCP7940N-RTC und wird in Einheiten von 5 Minuten gespeichert.
 * Referenzzeitpunkt ist z. B. Systemstart oder Inbetriebnahme.
 */
typedef uint32_t timestamp_t;

/**
 * @struct record_t
 * @brief Struktur eines Sensordatensatzes
 *
 * Wird im internen oder externen Flash gespeichert und über Funk übertragen.
 * Die Temperatur ist als Gleitkommazahl gespeichert, wird jedoch zur Übertragung konvertiert.
 */
#pragma pack(1)
typedef struct  {
    timestamp_t timestamp;  ///< Zeitstempel in 5-Minuten-Schritten
    float temperature;      ///< Temperatur in °C
    uint8_t flags;          ///< Statusbits (nur untere 4 Bit genutzt, z. B. CRC-valid, Sensorfehler)
} record_t;
#pragma pack()

/**
 * @enum mode_t
 * @brief Betriebsmodi des Sensorsystems (Zustandsmaschine)
 *
 * Diese Modi werden in der zentralen Zustandsmaschine verarbeitet.
 */
typedef enum {
    MODE_TEST,              ///< Interner Testmodus
    MODE_WAIT_FOR_ACTIVATION, ///< Wartet auf Funkaktivierung
    MODE_OPERATIONAL,       ///< Regulärer Messbetrieb
    MODE_PRE_HIGH_TEMP,      ///< Zwischenmodus vor Hochtemperaturphase (Alert-Überwachung)
    MODE_HIGH_TEMPERATURE,  ///< Spezieller Modus für die Hochtemperaturphase
    MODE_DATA_TRANSFER,     ///< Übertragung gespeicherter Daten via Funk
    MODE_SLEEP              ///< Energiesparmodus, wacht per RTC wieder auf
} mode_t;

#define MODE_COUNT 7

// === Modusnamen als String-Array ===
static const char* const mode_names[MODE_COUNT] = {
    [MODE_TEST]              = "TEST",
    [MODE_WAIT_FOR_ACTIVATION] = "WAIT_FOR_ACTIVATION",
    [MODE_OPERATIONAL]       = "OPERATIONAL",
    [MODE_PRE_HIGH_TEMP]      = "PRE_HIGH_TEMP",
    [MODE_HIGH_TEMPERATURE]  = "HIGH_TEMPERATURE",
    [MODE_DATA_TRANSFER]     = "DATA_TRANSFER",
    [MODE_SLEEP]             = "SLEEP"
};

/**
 * @brief Gibt den Namen eines Modus als String zurück.
 *
 * @param mode Modus
 * @return Zeiger auf String oder "UNKNOWN"
 */
static inline const char* mode_name(mode_t mode)
{
    if ((unsigned)mode < MODE_COUNT)
    {
        return mode_names[mode];
    }
    else
        return "UNKOWN";
}

/**
 * @enum event_t
 * @brief Asynchrone Ereignisse zur Steuerung der Zustandsmaschine
 *
 * Diese Events können von Interrupts oder Subsystemen ausgelöst werden,
 * um die Zustandsmaschine zu beeinflussen.
 */
typedef enum {
    EVENT_WAKEUP_TIMER,     ///< Wakeup durch RTC-Alarm
    EVENT_WAKEUP_TEMP_ALERT,///< Wakeup durch TMP126-Temperaturalarm
    EVENT_PING_RESPONSE,    ///< Empfangene Antwort auf Funk-Ping
    EVENT_DATA_SENT,        ///< Datentransfer abgeschlossen
    EVENT_LOW_BATTERY,      ///< Batteriespannung kritisch
    EVENT_RESET,            ///< System-Reset
    EVENT_MANUAL_TRIGGER    ///< Manueller Trigger (z. B. Taster)
} event_t;

#endif // TYPES_H
