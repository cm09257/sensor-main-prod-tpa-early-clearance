#include "stm8s.h"
#include "modes/mode_wait_for_activation.h"
// #include "modules/radio.h"
#include "modules/settings.h"
#include "modules/rtc.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "periphery/power.h"
#include "periphery/mcp7940n.h"
#include "config/config.h"
#include "app/state_machine.h"
#include "modules/packet_handler.h"

#define MAX_ACTIVATION_PING_SEND_RETRIES 3
#define ACK_TIMEOUT 1000
#define CMD_TIMEOUT 1000
#define DELAY_BEFORE_RETRY 10

void mode_wait_for_activation_run(void)
{
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
    DebugLn("=============== MODE_WAIT_FOR_ACTIVATION ===============");
#endif

    bool activation_successful = FALSE;

    while (1)
    {

//////////////// Send activation Ping
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
        DebugLn("[SENT] ACTIVATION_PING");
#endif
        uint8_t retry_count = 0;
        bool ack_received = FALSE;
        bool cmd_announced = FALSE;

        while ((!ack_received) && (retry_count < MAX_ACTIVATION_PING_SEND_RETRIES))
        {
            send_uplink_ping_for_activation(DEVICE_ID_MSB, DEVICE_ID_LSB);
            ack_received = wait_for_ack_by_gateway(ACK_TIMEOUT, &cmd_announced);
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
            if (ack_received)
                DebugLn("[RCVD] ACK Received by Gateway");
#endif

            if (cmd_announced)
            {
                uint8_t data[8];
                if (wait_for_cmd_by_gateway(CMD_TIMEOUT, data))
                {
                    if (data[1] == CMD_SET_RTC_OFFSET)
                    {
                        uint8_t day, month, hr, min, sec;
                        uint16_t year;
                        decode_downlink_cmd_set_rtc(data, &day, &month, &year, &hr, &min, &sec);

                        MCP7940N_Open();
                        MCP7940N_SetTime(hr, min, sec);
                        delay(1);
                        MCP7940N_SetDate(1, day, month, year);
                        MCP7940N_Close();
                        activation_successful = TRUE;
                        state_transition(MODE_PRE_HIGH_TEMP);
                        return;
                    }
                }
            }
            retry_count++;
            delay(DELAY_BEFORE_RETRY); /// Delay between pings in one try
        }

        ///////////// Prepare sleep and wakeup via RTC EXTI: disable interrupts.
        disableInterrupts();

        ///////////// Get current time
        uint8_t curr_hr, curr_min, curr_sec;
        MCP7940N_Open();
        MCP7940N_GetTime(&curr_hr, &curr_min, &curr_sec);

        ///////////// Alarm = current time + 1min
        uint8_t alarm_hr = curr_hr;
        uint8_t alarm_min = (curr_min + 1) % 60;

        if (alarm_min == 0)
            alarm_hr = (curr_hr + 1) % 24;

        MCP7940N_ConfigureAbsoluteAlarmX(0, alarm_hr, alarm_min, curr_sec);
        MCP7940N_Close();

        ///////////// Go to power_halt mode, wakeup using RTC EXTI
        mode_before_halt = MODE_WAIT_FOR_ACTIVATION;
        power_enter_halt();
        delay(10);
        enableInterrupts();

        __asm__("halt");

        ///////////// After wakeup: disable interrupts, clear alarm
        disableInterrupts();
        MCP7940N_Open();
        MCP7940N_ClearAlarmFlagX(0);
        MCP7940N_DisableAlarmX(0);
        MCP7940N_Close();
        
    }
}

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