#include "stm8s.h"
#include "modes/mode_wait_for_activation.h"
//#include "modules/radio.h"
#include "modules/settings.h"
#include "modules/rtc.h"
#include "utility/debug.h"
#include "periphery/power.h"
#include "config/config.h"
#include "app/state_machine.h"

#if DEV_MODE
#define PING_INTERVAL_MINUTES 0
#define PING_INTERVAL_FROM_NOW 30
#else
#define PING_INTERVAL_MINUTES 60
#endif

/*
void mode_wait_for_activation_run(void)
{
    DebugLn("=============== MODE_WAIT_FOR_ACTIVATION ===============");
    DebugLn("[MODE_WAIT_FOR_ACTIVATION] Starting periodic activation requests via RF...");

    uint8_t retry_count = 0;

    while (state_get_current() == MODE_WAIT_FOR_ACTIVATION)
    {
        DebugUVal("[MODE_WAIT_FOR_ACTIVATION] Sending activation request ping, try no. ", retry_count + 1, "...");

        // 1. Ping senden
        radio_send_ping(RADIO_PING_HEADER_ACTIVATION);

        // 2. Direkt im Anschluss Empfangsfenster öffnen
        bool activated = radio_receive_for_activation();

        // 3. Wenn aktiviert → Schleife verlassen
        if (activated)
        {
            DebugLn("[MODE_WAIT_FOR_ACTIVATION] Gateway has confirmed activation!");
            break;
        }

        DebugLn("[MODE_WAIT_FOR_ACTIVATION] No activation requested yet.");

        // 4. Nächsten Wakeup-Timer setzen
#if DEV_MODE
        DebugLn("[MODE_WAIT_FOR_ACTIVATION] Dev-Mode: Setting Alarm 1 to 30s later...");
        rtc_set_alarm_offset(RTC_ALARM_1, 0, PING_INTERVAL_FROM_NOW);
#else
        if (retry_count < 3)
        {
            DebugLn("[ACT] Setze nächsten Wakeup in 1 Minute (Alarm 1)");
            rtc_set_alarm_in_minutes(RTC_ALARM_1, 1);
            retry_count++;
        }
        else
        {
            DebugLn("[ACT] Keine Antwort in 3 Versuchen → Wechsel auf stündlichen Ping");
            rtc_set_relative_alarm(RTC_ALARM_1, 1, 0, 0);  // 1h später, absolut wäre auch möglich
            retry_count = 0;
        }
#endif

        // 5. Sleep bis RTC-Alarm
        DebugLn("[MODE_WAIT_FOR_ACTIVATION] Go to sleep until next RTC alarm.");
        mode_before_halt = MODE_WAIT_FOR_ACTIVATION;
        power_enter_halt();
        // TODO: enable interrupts, asm halt, etc...
    }

    DebugLn("[MODE_WAIT_FOR_ACTIVATION] Leaving MODE_WAIT_FOR_ACTIVATION...");
}
*/
void mode_wait_for_activation_run(void)
{
    nop();
}