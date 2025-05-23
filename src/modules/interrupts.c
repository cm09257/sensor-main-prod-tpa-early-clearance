#include "stm8s.h"
#include "modules/rtc.h"
#include "modules/sensor.h"
#include "periphery/power.h"
#include "utility/debug.h"
#include "app/state_machine.h"
#include "periphery/hardware_resources.h"
#include "modes/mode_pre_high_temperature.h" // ALERT-Flag

// === RTC Wakeup ISR ===

#if defined(PCB_REV_1_1)
INTERRUPT_HANDLER(EXTI1_IRQHandler, 5) // RTC_WAKE = PA1
#elif defined(PCB_REV_2_5)
INTERRUPT_HANDLER(EXTI3_IRQHandler, 9) // RTC_WAKE = PA3
#else
#error "Unbekannter RTC-Wake-Pin: kein EXTI-Handler definiert"
#endif
{
    bool triggered_0 = rtc_was_alarm_triggered(RTC_ALARM_0);
    bool triggered_1 = rtc_was_alarm_triggered(RTC_ALARM_1);

    if (triggered_0)
        rtc_clear_alarm(RTC_ALARM_0);
    if (triggered_1)
        rtc_clear_alarm(RTC_ALARM_1);

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

// === TMP126 Temperatur-IRQ (Temperaturgrenze überschritten) ===

#if defined(PCB_REV_1_1)
INTERRUPT_HANDLER(EXTI2_IRQHandler, 7) // TMP126 ALERT = PA2
#elif defined(PCB_REV_2_5)
INTERRUPT_HANDLER(EXTI5_IRQHandler, 13) // TMP126 ALERT = PE5
#else
#error "Unbekannter TMP_WAKE-Pin: kein EXTI-Handler definiert"
#endif
{
    DebugLn("[TMP126] Wakeup durch Temperaturalarm");
    pre_hi_temp_alert_triggered = TRUE;
}
