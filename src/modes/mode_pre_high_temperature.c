// src/modes/mode_pre_high_temperature.c
#include "stm8s_gpio.h"
#include "periphery/hardware_resources.h"
#include "app/state_machine.h"
#include "modes/mode_pre_high_temperature.h"
#include "modules/settings.h"
#include "modules/storage_internal.h"
#include "periphery/tmp126.h"
#include "utility/delay.h"

#include "utility/debug.h"

volatile bool pre_hi_temp_alert_triggered = FALSE;

void TMP126_DebugWakePin(void)
{

    GPIO_Init(TMP126_WAKE_PORT, TMP126_WAKE_PIN, GPIO_MODE_IN_PU_NO_IT);

    if (GPIO_ReadInputPin(TMP126_WAKE_PORT, TMP126_WAKE_PIN) == SET)
    {
        DebugLn("[TMP126 WAKE] High");
    }
    else
    {
        DebugLn("[TMP126 WAKE] Low");
    }
}

void mode_pre_high_temperature_run(void)
{
    Debug("=== Mode PRE_HIGH_TEMP] ===");
    if (!TMP126_OpenForAlert())
    {
        DebugLn("[PRE_HIGH_TEMP] Oeffnen des TMP126 für Alert fehlgeschlagen");
        state_transition(MODE_SLEEP);
        return;
    }
    // Alarmgrenze setzen

    DebugUVal("[PRE_HIGH_TEMP] Setting Hi Temp Threshold to ", PRE_HIGH_TEMP_THRESHOLD_C, "degC");
    TMP126_SetHiLimit(19); // TMP126_SetHiLimit(PRE_HIGH_TEMP_THRESHOLD_C);
    TMP126_SetLoLimit(-60);
    float read_hi_temp_float = TMP126_ReadHiLimit();
    float read_lo_temp_float = TMP126_ReadLoLimit();

 //   DebugFVal("[PRE_HIGH_TEMP] Readout high threshold = ", read_hi_temp_float, "degC");
    DebugFVal("[PRE_HIGH_TEMP] Readout low  threshold = ", read_lo_temp_float, "degC");

    TMP126_Enable_THigh_Alert();
    TMP126_Disable_TLow_Alert();
/*
    if (TMP126_Is_THigh_Enabled())
        DebugLn("[PRE_HIGH_TEMP] THigh Alert is Enabled.");
    else
        DebugLn("[PRE_HIGH_TEMP] THigh Alert is Disabled.");

    if (TMP126_Is_TLow_Enabled())
        DebugLn("[PRE_HIGH_TEMP] TLow Alert is Enabled.");
    else
        DebugLn("[PRE_HIGH_TEMP] TLow Alert is Disabled.");
        */

    uint16_t alarm_status = TMP126_ReadAlertStatus();
    DebugUVal("[PRE_HIGH_TEMP] Alert Status Register (dec. value) =", alarm_status, "");
    uint16_t configuation = TMP126_ReadConfigurationRaw();
    DebugUVal("[PRE_HIGH_TEMP] Configuration Register (dec. value) =", configuation, "");

    char buf[32];
    TMP126_OpenForMeasurement();
    TMP126_Format_Temperature(buf);
    TMP126_CloseForMeasurement();
    DebugLn(buf);

    TMP126_DebugWakePin();

#ifdef PRE_HIGH_TEMP_MEASURE
    DebugLn("[PRE_HIGH_TEMP] Zyklische Messung aktiv");

    float temp = sensor_read_temperature();
    int temp_int = (int)(temp * 100); // falls gewünscht: 2 Nachkommastellen
    DebugIVal("[PRE_HIGH_TEMP] Temperatur: ", temp_int, " x0.01 C");

    if (storage_internal_add_measurement(temp))
    {
        DebugLn("[PRE_HIGH_TEMP] Temperatur gespeichert");
    }
    else
    {
        DebugLn("[PRE_HIGH_TEMP] Speicher voll oder Fehler");
    }

    state_set_mode(MODE_SLEEP);
#else
    DebugLn("[PRE_HIGH_TEMP] Warte auf TMP126 Alert (EXTI)");
    TMP126_DebugWakePin();

    if (pre_hi_temp_alert_triggered)
    {
        pre_hi_temp_alert_triggered = FALSE;
        DebugLn("[PRE_HIGH_TEMP] ALERT erkannt -> Wechsel in MODE_HIGH_TEMPERATURE");
        TMP126_CloseForAlert();
        state_transition(MODE_HIGH_TEMPERATURE);
    }
    else
    {
        state_transition(MODE_SLEEP);
    }
#endif
}
