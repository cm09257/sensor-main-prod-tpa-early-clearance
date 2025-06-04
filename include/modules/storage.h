#ifndef STORAGE_H
#define STORAGE_H

#include "stm8s.h"
#include <stdint.h>
#include "common/types.h"

/// @file storage.h
/// @brief Schnittstelle für EEPROM- und Flash-Speicherfunktionen

// -----------------------------------------------------------------------------
// EEPROM
// -----------------------------------------------------------------------------

/**
 * @brief Entsperrt den EEPROM-Zugriff (erforderlich vor Schreiben).
 */
void storage_eeprom_unlock(void);

/**
 * @brief Schreibt Daten in den EEPROM.
 * @param address Startadresse im EEPROM
 * @param data Zeiger auf die zu schreibenden Bytes
 * @param len Anzahl der zu schreibenden Bytes
 */
void storage_write_eeprom(uint16_t address, const uint8_t* data, uint16_t len);

/**
 * @brief Liest Daten aus dem EEPROM.
 * @param address Startadresse im EEPROM
 * @param data Zielpuffer für gelesene Daten
 * @param len Anzahl der zu lesenden Bytes
 * @return TRUE bei Erfolg, sonst FALSE
 */
bool storage_read_eeprom(uint16_t address, uint8_t* data, uint16_t len);

// -----------------------------------------------------------------------------
// Persistenter Betriebsmodus
// -----------------------------------------------------------------------------

/**
 * @brief Speichert den aktuellen Zustand im EEPROM mit CRC.
 * @param mode Betriebsmodus
 */
void persist_current_mode(mode_t mode);

/**
 * @brief Lädt den letzten gespeicherten Betriebsmodus.
 * @param[out] out_mode Zeiger auf den geladenen Modus
 * @return TRUE wenn CRC gültig war, sonst FALSE
 */
bool load_persisted_mode(mode_t* out_mode);

// -----------------------------------------------------------------------------
// Geräte-ID (4 Byte)
// -----------------------------------------------------------------------------

/**
 * @brief Liest die gespeicherte Geräte-ID aus dem EEPROM.
 * @param[out] out Puffer (mind. 4 Byte) für die ID
 * @return TRUE bei Erfolg, sonst FALSE
 */
bool load_device_id(uint8_t* out);

/**
 * @brief Speichert einmalig eine Geräte-ID, falls noch nicht gesetzt.
 * @param id Zeiger auf die 4-Byte-ID
 * @return TRUE bei Erfolg, FALSE wenn ID schon gesetzt oder Fehler
 */
bool store_device_id(const uint8_t* id);

// -----------------------------------------------------------------------------
// Externer Flash-Datenspeicher
// -----------------------------------------------------------------------------

/**
 * @brief Schreibt einen Datensatz (record_t) in den Flash.
 * @param rec Zeiger auf Datensatz
 * @return TRUE bei Erfolg, FALSE bei Fehler
 */
bool flash_write_record(const record_t* rec);

/**
 * @brief Liest einen Datensatz aus dem Flash.
 * @param index Index des Datensatzes (0 = ältester)
 * @param[out] out Zeiger auf Zielstruktur
 * @return TRUE bei Erfolg, FALSE bei ungültigem Index oder CRC-Fehler
 */
bool flash_read_record(uint16_t index, record_t* out);

/**
 * @brief Gibt die Anzahl aktuell gespeicherter Datensätze im Flash zurück.
 * @return Anzahl gültiger Datensätze
 */
uint16_t flash_get_count(void);

// -----------------------------------------------------------------------------
// Interner Pufferspeicher (z. B. bei Stromausfall oder High-Temp)
// -----------------------------------------------------------------------------

/**
 * @brief Kopiert alle internen Datensätze ins externe Flash.
 * Wird z. B. nach dem HIGH_TEMPERATURE-Modus aufgerufen.
 * @return TRUE bei Erfolg, sonst FALSE
 */
bool copy_internal_to_external_flash(void);

//void storage_debug_dump_records(void);
void storage_flash_test(void);
#endif // STORAGE_H
