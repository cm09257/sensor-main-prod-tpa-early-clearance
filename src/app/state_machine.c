/**
 * @file state_machine.c
 * @brief Ablaufsteuerung über Zustandsmaschine (State Machine)
 *
 * Dieses Modul koordiniert die Betriebsmodi des Sensorsystems.
 * Zustände: TEST, WAIT_FOR_ACTIVATION, OPERATIONAL, HIGH_TEMPERATURE, DATA_TRANSFER, SLEEP.
 */

#include "stm8s.h"
#include "stm8s_gpio.h"
#include "periphery/hardware_resources.h"
#include "app/state_machine.h"
#include "modules/radio.h"
#include "modules/settings.h"
#include "modules/uplink_builder.h"
#include "modules/storage.h"
#include "config/config.h"
#include "utility/delay.h"
#include "utility/debug.h"
#include "utility/random.h"
#include "periphery/power.h"
#include "periphery/tmp126.h"
#include "production/production_test.h"
#include "modes/mode_test.h"
#include "modes/mode_operational.h"
#include "modes/mode_data_transfer.h"
#include "modes/mode_pre_high_temperature.h"
#include "modes/mode_wait_for_activation.h"
#include "modes/mode_high_temperature.h"

// === Initialisierung ===

/**
 * @brief Initialisiert den Zustand des Systems aus persistentem Speicher.
 *
 * Lädt den letzten bekannten Betriebsmodus (z. B. WAIT_FOR_ACTIVATION) aus EEPROM,
 * oder geht in MODE_TEST, wenn ungültig oder nicht vorhanden.
 */
void state_init(void)
{
    mode_t persisted;

    if (load_persisted_mode(&persisted)) // persisted mode stored in eeprom ?
    {
        if (persisted <= MODE_SLEEP)
        {
            current_mode = persisted;
            DebugUVal("Start in gespeicherten Modus", current_mode, "");
        }
        else
        {
            current_mode = MODE_TEST;
            DebugLn("Persistierter Modus ungueltig, gehe in MODE_TEST");
        }
    }
    else
    {
        current_mode = MODE_TEST;
        DebugLn("Kein gespeicherter Modus gefunden, starte im MODE_TEST");
    }

    last_measurement_ts = 0;
    mode_transition_pending = FALSE;
}

// === Hauptverarbeitung ===

/**
 * @brief Haupt-Dispatcher: Führt den Code des aktuellen Modus aus.
 *
 * Diese Funktion wird in jedem Hauptschleifendurchlauf (`main()`) aufgerufen.
 * Nach Rückkehr aus dem aktiven Modus wird geprüft, ob ein Moduswechsel ansteht.
 */
void state_process(void)
{
    
 //   DebugLn("[sensor-main-state-machine] In state_process.");
 //   DebugUVal("[sensor-main-state-machine] current_mode = ",current_mode,"");
    switch (current_mode)
    {
    case MODE_TEST:
        mode_test_run();
        break;

    case MODE_WAIT_FOR_ACTIVATION:
 //       mode_wait_for_activation_run();
        break;

    case MODE_PRE_HIGH_TEMP:
        mode_pre_high_temperature_run();
        break;

    case MODE_HIGH_TEMPERATURE:
        mode_high_temperature_run();
        break;

    case MODE_OPERATIONAL:
        mode_operational_run();
        break;

    case MODE_DATA_TRANSFER:
        mode_data_transfer_run();
        break;

    case MODE_SLEEP:
        DebugLn("Enter Mode Sleep");
        while(1)
        {
            delay(50);
        }
        //power_enter_halt(); // verlässt Funktion erst nach Wakeup
        break;
    }

    if (mode_transition_pending)
    {
        current_mode = next_mode;
        mode_transition_pending = FALSE;
        // Optional: Debug-Ausgabe oder Ereignislog
    }
}

void set_mode_debug_only(mode_t new_mode)
{
    DebugUVal("[sensor-main-state-machine] Debug-Only setting of mode to ", new_mode, "");
    current_mode = new_mode;
}

// === Moduswechsel ===

/**
 * @brief Markiert einen Übergang in einen neuen Modus.
 *
 * Der eigentliche Übergang erfolgt am Ende von `state_process()` (nicht sofort!).
 * Persistente Modi wie `OPERATIONAL` und `WAIT_FOR_ACTIVATION` werden gespeichert.
 *
 * @param new_mode Zielmodus
 */
void state_transition(mode_t new_mode)
{
    if (new_mode == current_mode)
        return;

    // Persistiere nur "langfristige" Modi
    switch (new_mode)
    {
    case MODE_OPERATIONAL:
    case MODE_PRE_HIGH_TEMP:
    case MODE_WAIT_FOR_ACTIVATION:
        persist_current_mode(new_mode);
        break;
    default:
        // TEST, HIGH_TEMPERATURE, SLEEP, DATA_TRANSFER → nicht persistieren
        break;
    }

    next_mode = new_mode;
    mode_transition_pending = TRUE;
}

/**
 * @brief Gibt den aktuellen Zustand zurück.
 *
 * @return Aktueller Betriebsmodus
 */
mode_t state_get_current(void)
{
    return current_mode;
}
