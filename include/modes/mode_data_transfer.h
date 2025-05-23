// mode_data_transfer.h
#ifndef MODE_DATA_TRANSFER_H
#define MODE_DATA_TRANSFER_H

/**
 * @file mode_data_transfer.h
 * @brief Schnittstelle für den Datenübertragungsmodus
 *
 * Dieser Modus wird aufgerufen, wenn der Sensor gespeicherte
 * Messwerte via Funk an das Gateway überträgt.
 * Die Funktion übernimmt Paketierung, Übertragung mit ACK/NACK-
 * Verarbeitung und ggf. Wiederholungsversuche.
 */

/**
 * @brief Hauptfunktion des Datenübertragungsmodus
 *
 * Liest Datensätze aus dem Flash-Speicher, baut Uplink-Pakete und
 * sendet diese über das Funkmodul. Bei Erfolg werden alle Daten
 * übertragen, andernfalls wird die Übertragung abgebrochen.
 */
void mode_data_transfer_run(void);

#endif // MODE_DATA_TRANSFER_H
