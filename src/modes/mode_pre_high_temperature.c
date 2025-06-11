// src/modes/mode_pre_high_temperature.c
#include "stm8s_gpio.h"
#include "stm8s_exti.h"
#include "periphery/hardware_resources.h"
#include "app/state_machine.h"
#include "modes/mode_pre_high_temperature.h"
#include "modules/settings.h"
#include "modules/rtc.h"
#include "periphery/power.h"
#include "periphery/tmp126.h"
#include "utility/delay.h"
#include "utility/debug.h"

#define PRE_HI_TEMP_MEAS_INTERVAL_IN_MIN 2

static bool pre_hi_temp_interrupt_configured = FALSE;
volatile bool pre_hi_temp_alert_triggered = FALSE;

void mode_pre_high_temperature_run(void)
{
    DebugLn("=============== MODE_PRE_HIGH_TEMP ===============");

    ////////////// Configuring TMP126_WAKE pin for EXTI
    GPIO_Init(TMP126_WAKE_PORT, TMP126_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(TMP126_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    ////////////// Configuring Temp Hi Alert on TMP126
    TMP126_OpenForAlert();
    TMP126_SetHiLimit(PRE_HIGH_TEMP_THRESHOLD_C);
    TMP126_SetHysteresis(1.0f);
    TMP126_Disable_TLow_Alert();
    TMP126_Enable_THigh_Alert();

    ////////////// Output current temp and time
    char buf[32];
    rtc_get_format_time(buf);    
    DebugLn(buf);
    TMP126_Format_Temperature(buf);
    DebugLn(buf);

    ////////////// Set sleep mode and wait for TMP_WAKE EXTI
    power_enter_halt();
    delay(100);
    enableInterrupts();
    __asm__("halt");
    
    ////////////// Woke up from EXTI
    DebugLn("======================== Hi Alert Triggered ===");
    disableInterrupts();

    ////////////// Disable alert and TMP126
    TMP126_Disable_THigh_Alert();
    TMP126_CloseForAlert();

    ////////////// --> MODE_HIGH_TEMPERATURE
    state_transition(MODE_HIGH_TEMPERATURE);
}
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

#endif
//}
