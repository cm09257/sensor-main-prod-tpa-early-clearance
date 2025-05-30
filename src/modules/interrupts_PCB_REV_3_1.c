#include "stm8s.h"
#include "stm8s_exti.h"
#include "stm8s_itc.h"
#include "stm8s_gpio.h"
#include "app/state_machine.h"
#include "modules/interrupts_PCB_REV_3_1.h"
#include "modes/mode_pre_high_temperature.h" // ALERT-Flag
#include "periphery/power.h"
#include "periphery/hardware_resources.h"
#include "periphery/mcp7940n.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "periphery/tmp126.h"

#if defined(PCB_REV_3_1)

// === RTC Wakeup ISR ===

INTERRUPT_HANDLER(EXTI_PORT_D_IRQHandler, PORT_D_INTERRUPT_VECTOR) // RTC_WAKE = PD2
{
    bool triggered_0 = MCP7940N_IsAlarm0Triggered(); // rtc_was_alarm_triggered(RTC_ALARM_0);
    bool triggered_1 = MCP7940N_IsAlarm1Triggered(); //(RTC_ALARM_1);

    MCP7940N_DisableAlarmX(0);   // immediately disable and clear alarms
    MCP7940N_ClearAlarmFlagX(0); // in ISR
    MCP7940N_DisableAlarmX(1);   // immediately disable and clear alarms
    MCP7940N_ClearAlarmFlagX(1); // in ISR

    Debug("Wakeup durch Alarm: ");
    if (triggered_0)
        Debug("ALM0 ");
    if (triggered_1)
        Debug("ALM1 ");
    DebugLn("");

    switch (mode_before_halt)
    {
    case MODE_HIGH_TEMPERATURE:
        DebugLn("Wakeup aus HIGH_TEMPERATURE");
        break;
    case MODE_OPERATIONAL:
        DebugLn("Wakeup aus OPERATIONAL");
        break;
    case MODE_WAIT_FOR_ACTIVATION:
        DebugLn("Wakeup aus WAIT_FOR_ACTIVATION");
        break;
    case MODE_SLEEP:
        DebugLn("Wakeup aus SLEEP");
        break;
    default:
        DebugLn("Wakeup aus unbekanntem Modus");
        break;
    }

    if (mode_before_halt == MODE_SLEEP || state_get_current() == MODE_SLEEP)
        state_transition(MODE_OPERATIONAL);
}

INTERRUPT_HANDLER(EXTI_PORT_C_IRQHandler, PORT_C_INTERRUPT_VECTOR) // TMP126 ALERT = PE5
{    
    pre_hi_temp_alert_triggered = TRUE;
}
#endif