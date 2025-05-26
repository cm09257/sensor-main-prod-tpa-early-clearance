#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include "stm8s.h"
#include "common/types.h"


// === Interner Zustand ===

/// Aktueller Modus (default: TEST)
static mode_t current_mode = MODE_TEST;

/// Letzter Zeitstempel einer Messung (für spätere Plausibilitätsprüfungen)
static timestamp_t last_measurement_ts = 0;

/// Wird gesetzt, wenn ein Zustandswechsel am Ende von state_process() erfolgen soll
static bool mode_transition_pending = FALSE;

/// Modus vor Sleep (z. B. zur Rückkehr nach TEMP_ALERT)
static mode_t mode_before_halt = MODE_TEST;

/// Vormerkung für nächsten Modus (bei Übergang)
static mode_t next_mode;


/**
 * @file state_machine.h
 * @brief Zentrale Zustandsmaschine des Systems
 *
 * Dieses Modul verwaltet den aktuellen Betriebsmodus (mode_t) und die 
 * Übergänge zwischen Zuständen. Es wird zyklisch aus `main()` aufgerufen 
 * und entscheidet je nach Modus, welches Submodul aktiv ist.
 *
 * Unterstützte Modi:
 * - MODE_TEST
 * - MODE_WAIT_FOR_ACTIVATION
 * - MODE_HIGH_TEMPERATURE
 * - MODE_OPERATIONAL
 * - MODE_DATA_TRANSFER
 * - MODE_SLEEP
 */

/**
 * @brief Initialisiert den Zustand beim Systemstart.
 * 
 * Lädt ggf. den zuletzt gespeicherten Modus aus EEPROM und bereitet 
 * die Zustandsmaschine vor. Muss einmalig beim Systemstart aufgerufen werden.
 */
void state_init(void);

/**
 * @brief Haupt-Dispatcher der Zustandsmaschine.
 * 
 * Ruft die passende Funktion (z. B. `mode_operational_run()`) je nach aktuellem Modus auf.
 * Wird kontinuierlich in der Main-Loop ausgeführt.
 */
void state_process(void);

/**
 * @brief Führt einen expliziten Zustandswechsel durch.
 * 
 * Wechselt den aktuellen Modus zu `new_mode` und speichert diesen persistiert.
 * Auch interne Variablen (z. B. `mode_before_halt`) werden entsprechend aktualisiert.
 * 
 * @param new_mode Der Zielzustand
 */
void state_transition(mode_t new_mode);

/**
 * @brief Gibt den aktuell aktiven Modus zurück.
 * 
 * @return Der aktuelle Betriebsmodus
 */
mode_t state_get_current(void);

#endif // STATE_MACHINE_H
