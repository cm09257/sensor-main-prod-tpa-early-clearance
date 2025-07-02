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

static bool pre_hi_temp_interrupt_configured = FALSE;
volatile bool pre_hi_temp_alert_triggered = FALSE;

void mode_pre_high_temperature_run(void)
{
#if defined(DEBUG_MODE_PRE_HI_TEMP)
    DebugLn("=MD_PRE_HI_TMP=");
#endif

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
#if defined(DEBUG_MODE_PRE_HI_TEMP)
    char buf[32];
    rtc_get_format_time(buf);
    DebugLn(buf);
    TMP126_Format_Temperature(buf);
    DebugLn(buf);
#endif

#if defined(ITS_TOO_HOT)
    DebugLn("=[ITS_TOO_HOT]Hi Alrt Faked=");
    state_transition(MODE_HIGH_TEMPERATURE);
    return;
#endif

    ////////////// Set sleep mode and wait for TMP_WAKE EXTI
    power_enter_halt();
    delay(100);
    enableInterrupts();
    __asm__("halt");

////////////// Woke up from EXTI
#if defined(DEBUG_MODE_PRE_HI_TEMP)
    DebugLn("=Hi Alrt Trigd=");
#endif
    disableInterrupts();

    ////////////// Disable alert and TMP126
    TMP126_Disable_THigh_Alert();
    TMP126_CloseForAlert();

    ////////////// --> MODE_HIGH_TEMPERATURE
    state_transition(MODE_HIGH_TEMPERATURE);
}
//}
