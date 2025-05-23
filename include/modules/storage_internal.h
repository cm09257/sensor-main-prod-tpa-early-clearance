#ifndef STORAGE_INTERNAL_H
#define STORAGE_INTERNAL_H

#include <stdint.h>
#include "stm8s.h"
#include "common/types.h"  // record_t
#include "app/state_machine.h" // falls wir mode_t brauchen

/**
 * @brief  Initialisiert das interne Flash-Speichermodul.
 */
void internal_storage_init(void);

/**
 * @brief  Schreibt einen Datensatz ins interne Flash.
 *         Achtung auf Lebensdauer und freies Erase-Handling!
 * @param  rec  Zeiger auf record_t (Zeit + Temp + Flags)
 * @return true bei Erfolg, false bei Fehler
 */
bool internal_flash_write_record(const record_t* rec);

/**
 * @brief  Liest einen Datensatz anhand eines Index.
 * @param  index Datensatzindex (0..n-1)
 * @param  out   Zeiger auf record_t
 * @return true bei Erfolg, false bei Fehler
 */
bool internal_flash_read_record(uint16_t index, record_t* out);

/**
 * @brief  Anzahl der Datensätze im internen Speicher.
 */
uint16_t internal_flash_get_count(void);

/**
 * @brief  Löscht alle intern gespeicherten Datensätze.
 */
void internal_flash_erase_all(void);

#endif // STORAGE_INTERNAL_H
