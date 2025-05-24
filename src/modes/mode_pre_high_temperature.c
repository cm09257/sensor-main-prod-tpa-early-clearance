// src/modes/mode_pre_high_temperature.c
#include "app/state_machine.h"
#include "modes/mode_pre_high_temperature.h"
#include "modules/settings.h"
#include "modules/storage_internal.h"
#include "periphery/tmp126.h"

#include "utility/debug.h"

volatile bool pre_hi_temp_alert_triggered = FALSE;

void mode_pre_high_temperature_run(void)
{
    Debug("[PRE_HIGH_TEMP] Start");

    // Alarmgrenze setzen
    TMP126_SetHiLimit(PRE_HIGH_TEMP_THRESHOLD_C);
    TMP126_Enable_THigh_Alert();
    TMP126_Disable_TLow_Alert();
    
#ifdef PRE_HIGH_TEMP_MEASURE
    DebugLn("[PRE_HIGH_TEMP] Zyklische Messung aktiv");

    float temp = sensor_read_temperature();
    int temp_int = (int)(temp * 100);  // falls gewünscht: 2 Nachkommastellen
    DebugVal("[PRE_HIGH_TEMP] Temperatur: ", temp_int, " x0.01 C");

    if (storage_internal_add_measurement(temp)) {
        DebugLn("[PRE_HIGH_TEMP] Temperatur gespeichert");
    } else {
        DebugLn("[PRE_HIGH_TEMP] Speicher voll oder Fehler");
    }

    state_set_mode(MODE_SLEEP);
#else
    DebugLn("[PRE_HIGH_TEMP] Warte auf TMP126 Alert (EXTI)");

    if (pre_hi_temp_alert_triggered) {
        pre_hi_temp_alert_triggered = FALSE;
        DebugLn("[PRE_HIGH_TEMP] ALERT erkannt → Wechsel in MODE_HIGH_TEMPERATURE");
        state_transition(MODE_HIGH_TEMPERATURE);
    } else {
        state_transition(MODE_SLEEP);
    }
#endif
}
