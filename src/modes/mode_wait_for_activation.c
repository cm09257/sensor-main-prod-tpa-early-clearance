#include "stm8s.h"
#include "stm8s_gpio.h"
#include "stm8s_exti.h"
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
#include "periphery/hardware_resources.h"

#define MAX_ACTIVATION_PING_SEND_RETRIES 3
#define ACK_TIMEOUT 20
#define CMD_TIMEOUT 20
#define DELAY_BEFORE_RETRY 10

void mode_wait_for_activation_run(void)
{
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
    DebugLn("=MODE_WAIT_FOR_ACTIVATION=");
#endif

    ///////////// Configuring RTC_WAKE pin for EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

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

            if (ack_received)
            {
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
                DebugLn("[RCVD] ACK_BY_GATEWAY");
#endif

                if (cmd_announced)
                {
                    uint8_t data[8];
                    if (wait_for_cmd_by_gateway(CMD_TIMEOUT, data))
                    {
                        if (data[1] == CMD_SET_RTC_OFFSET)
                        {
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
                            DebugLn("[RCVD] CMD_SET_RTC");
#endif
                            uint8_t day, month, hr, min, sec;
                            uint16_t year;
                            decode_downlink_cmd_set_rtc(data, &day, &month, &year, &hr, &min, &sec);

                            MCP7940N_Open();
                            MCP7940N_SetTime(hr, min, sec);
                            delay(1);
                            MCP7940N_SetDate(1, day, month, year);
                            MCP7940N_Close();

#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
                            uint8_t read_weekday, read_day, read_month, read_year;
                            uint8_t read_hour, read_min, read_sec;
                            MCP7940N_Open();
                            MCP7940N_GetDate(&read_weekday, &read_day, &read_month, &read_year);
                            MCP7940N_GetTime(&read_hour, &read_min, &read_sec);
                            MCP7940N_Close();
                            char buf[32];
                            sprintf(buf, "%02u.%02u.%02u %02u:%02u:%02u", read_day, read_month, read_year, read_hour, read_min, read_sec);
                            DebugLn(buf);
#endif

                            activation_successful = TRUE;
                            state_transition(MODE_PRE_HIGH_TEMP);

                            return;
                        }
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
        MCP7940N_EnableAlarmX(0);
        MCP7940N_DisableAlarmX(1);
        MCP7940N_ClearAlarmFlagX(1);
        MCP7940N_ClearAlarmFlagX(0);
        MCP7940N_Close();

        ///////////// Go to power_halt mode, wakeup using RTC EXTI
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
        char buf[32];
        sprintf(buf, "%02u:%02u:%02u-%02u:%02u:%02u", curr_hr, curr_min, curr_sec, alarm_hr, alarm_min, curr_sec);
        DebugLn(buf);
#endif

        mode_before_halt = MODE_WAIT_FOR_ACTIVATION;
        power_enter_halt();
        delay(2000);
        enableInterrupts();

        __asm__("halt");

        ///////////// After wakeup: disable interrupts, clear alarm
        disableInterrupts();
        MCP7940N_Open();
        MCP7940N_ClearAlarmFlagX(0);
        MCP7940N_ClearAlarmFlagX(1);
        MCP7940N_DisableAlarmX(0);
        MCP7940N_DisableAlarmX(1);
        MCP7940N_Close();
    }
}