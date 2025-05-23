#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Globale Konfigurationsparameter für Build und Kommunikation
 *
 * Diese Datei enthält globale Schalter für Entwicklungsmodus,
 * Debug-Ausgaben und Funkprotokollparameter.
 */

/**
 * @def DEV_MODE
 * @brief Entwicklungsmodus aktivieren
 *
 * Aktiviert verkürzte Zeitintervalle und Debugverhalten (z. B. schnellere Pings, kürzere Wartezeiten).
 * Sollte im Produktivbetrieb deaktiviert werden (DEV_MODE = 0).
 */
#define DEV_MODE 1

/**
 * @def DEBUG
 * @brief Debug-Ausgaben aktivieren
 *
 * Schaltet printf-/Debug-Ausgaben ein. Hat Auswirkungen auf UART-Initialisierung.
 */
#define DEBUG 1

/**
 * @def UPLINK_OVERHEAD_BYTES
 * @brief Anzahl Bytes, die für Header und Metadaten im Uplink reserviert sind
 *
 * Enthält:
 * - 1 Byte Header (z. B. 0xA2)
 * - 1 Byte Sequenznummer
 * - 4 Byte Geräte-ID
 * - 1 Byte Payload-Länge
 * - 1 Byte CRC-8
 */
#define UPLINK_OVERHEAD_BYTES 7

/**
 * @def MAX_UPLINK_PACKET_SIZE
 * @brief Maximale Paketgröße für einen Funk-Uplink
 *
 * Berechnet aus Overhead + Nutzdaten (5 Byte pro Messwert).
 * MAX_RECORDS_PER_PACKET wird an anderer Stelle definiert (z. B. radio.h).
 */
#define MAX_UPLINK_PACKET_SIZE (UPLINK_OVERHEAD_BYTES + MAX_RECORDS_PER_PACKET * 5)

/**
 * @def MAX_RADIO_ATTEMPTS
 * @brief Maximale Anzahl Wiederholungen beim Senden eines Funkpakets (ACK erwartet)
 */
#define MAX_RADIO_ATTEMPTS 3

/**
 * @def MAX_RESEND_DELAY_MS
 * @brief Maximale zufällige Wartezeit (ms) zwischen Wiederholungsversuchen bei Funkfehler
 *
 * Eine zufällige Wartezeit (0 … MAX_RESEND_DELAY_MS) wird vor dem erneuten Senden eines Pakets verwendet.
 */
#define MAX_RESEND_DELAY_MS 1000

#endif // CONFIG_H
