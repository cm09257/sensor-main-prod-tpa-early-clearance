// src/modes/mode_pre_high_temperature.c
#include "stm8s_gpio.h"
#include "stm8s_exti.h"
#include "periphery/hardware_resources.h"
#include "app/state_machine.h"
#include "modes/mode_pre_high_temperature.h"
#include "modules/settings.h"
#include "modules/storage_internal.h"
#include "modules/rtc.h"
#include "periphery/power.h"
#include "periphery/mcp7940n.h"
#if defined(PCB_REV_2_5)
#include "modules/interrupts_PCB_REV_2_5.h"
#elif defined(PCB_REV_3_1)
#include "modules/interrupts_PCB_REV_3_1.h"
#endif
#include "periphery/tmp126.h"
#include "utility/delay.h"

#include "utility/debug.h"

#define PRE_HI_TEMP_MEAS_INTERVAL_IN_MIN 2

static bool pre_hi_temp_interrupt_configured = FALSE;
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

void increase_time_by_min(uint8_t delta_min, uint8_t curr_h, uint8_t curr_m, uint8_t curr_s, uint8_t *new_h, uint8_t *new_m, uint8_t *new_s)
{
    uint16_t total_minutes = curr_h * 60 + curr_m + delta_min;
    *new_h = (total_minutes / 60) % 24;
    *new_m = total_minutes % 60;
    *new_s = curr_s; // Sekunden bleiben gleich
}

void mode_pre_high_temperature_run(void)
{
    DebugLn("=== Mode PRE_HIGH_TEMP ===");

    float curr_temp;

    // Configuring TMP126 EXTI
    GPIO_Init(TMP126_WAKE_PORT, TMP126_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(TMP126_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    // Configuring Temp Hi Alert on TMP126
    TMP126_OpenForAlert();
    TMP126_SetHiLimit(PRE_HIGH_TEMP_THRESHOLD_C);
    TMP126_SetHysteresis(1.0f);
    TMP126_Disable_TLow_Alert();
    TMP126_Enable_THigh_Alert();
    //Debug_I2C_PinModes();
    char buf[32];
    rtc_get_format_time(buf);
    DebugLn(buf);
  //  Debug_I2C_PinModes();
    /// For Production: Version with     power_enter_halt();
    power_enter_halt();
    delay(100);

    enableInterrupts();

    __asm__("halt");
    //
    DebugLn("======================== Hi Alert Triggered ===");
    TMP126_Disable_THigh_Alert();
    TMP126_CloseForAlert();
    disableInterrupts();
    state_transition(MODE_HIGH_TEMPERATURE);

    /* /// For Debug: Loop without ___HALT___
    DebugLn("[PRE_HI_TEMP_MODE] Waiting for Hi-Temp Alert Interrupt ...");
    char buf[32];

    // Wait for interrupt/enter HALT mode for power save
    while (1)  // TODO: Replace loop by power_enter_halt() once function is completed.
    {
        TMP126_Format_Temperature(buf);
        DebugLn(buf);
        delay(1000);
        if (pre_hi_temp_alert_triggered)
        {
            disableInterrupts();
            TMP126_Disable_THigh_Alert();
            DebugLn("======================== Hi Alert Triggered ===");
            TMP126_CloseForAlert();
            state_transition(MODE_HIGH_TEMPERATURE);
            return;
        }
        nop();
    }  */
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
