#include "stm8s.h"
#include "modes/mode_wait_for_activation.h"
#include "modules/radio.h"
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

void mode_wait_for_activation_run(void)
{
    DebugLn("=== MODE_WAIT_FOR_ACTIVATION ===");
    DebugLn("[ACT] Starte zyklischen Aktivierungsversuch per Funk...");

    uint8_t retry_count = 0;

    while (state_get_current() == MODE_WAIT_FOR_ACTIVATION)
    {
        DebugVal("[ACT] Sende Aktivierungs-Ping, Versuch", retry_count + 1, "");

        // 1. Ping senden
        radio_send_ping(RADIO_PING_HEADER_ACTIVATION);

        // 2. Direkt im Anschluss Empfangsfenster öffnen
        bool activated = radio_receive_for_activation();

        // 3. Wenn aktiviert → Schleife verlassen
        if (activated)
        {
            DebugLn("[ACT] Gateway hat Aktivierung bestätigt!");
            break;
        }

        DebugLn("[ACT] Noch keine Aktivierung erhalten");

        // 4. Nächsten Wakeup-Timer setzen
#if DEV_MODE
        DebugLn("[ACT] Dev-Mode: Setze Alarm 1 auf 30 s später");
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
        DebugLn("[ACT] Gehe in Sleep bis nächster RTC-Alarm (Alarm 1)");
        sleep_until_rtc();
    }

    DebugLn("[ACT] Verlasse MODE_WAIT_FOR_ACTIVATION");
}
