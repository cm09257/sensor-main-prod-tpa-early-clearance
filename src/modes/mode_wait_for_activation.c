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
#include "periphery/RFM69.h"
#include "config/config.h"
#include "app/state_machine.h"
#include "modules/packet_handler.h"
#include "periphery/hardware_resources.h"

#define MAX_ACTIVATION_PING_SEND_RETRIES 3
#define ACK_TIMEOUT 1000
#define CMD_TIMEOUT 1000
#define DELAY_BEFORE_RETRY 10

void mode_wait_for_activation_run(void)
{
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
    DebugLn("=MD_WAIT_FR_ACT=");
#endif

    ///////////// Configuring RTC_WAKE pin for EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    bool activation_successful = FALSE;

    while (1)
    {

//////////////// Send activation Ping
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
        DebugLn("[SENT]ACT_PNG");
#endif
        uint8_t retry_count = 0;
        bool ack_received = FALSE;
        bool cmd_announced = FALSE;

        while ((!ack_received) && (retry_count < MAX_ACTIVATION_PING_SEND_RETRIES))
        {
            RFM69_open();
            send_uplink_ping_for_activation(DEVICE_ID_MSB, DEVICE_ID_LSB);
            ack_received = wait_for_ack_by_gateway(ACK_TIMEOUT, &cmd_announced);

            if (ack_received)
            {
#if defined(DEBUG_MODE_WAIT_FOR_ACTIVATION)
                DebugLn("[RCVD]ACKBYGTWY");
#endif

                if (cmd_announced)
                {
                    DebugLn("Wt fr cmd");
                    uint8_t rx_data[8];
                    RFM69_WriteReg(RFM_REG_IRQ_FLAGS2, 0x10); // FIFOReset
                    RFM69_SetModeRx();                        // neuer Sync

                    uint16_t timeout = 0;
                    while (timeout < 100) /////// RECEIVE LOOP Wait For Command
                    {
                        ///////////// Receive packet
                        bool ok = RFM69_ReceiveFixed8BytesECC(rx_data, 200);

                        ///////////// Extract header/cmd
                        uint8_t header_byte = rx_data[0];
                        uint8_t cmd_byte = rx_data[1];

                        ///////////// CMD_SET_RTC_OFFSET?
                        if ((header_byte == DOWNLINK_HEADER) && (cmd_byte == CMD_SET_RTC_OFFSET))
                        {

                            ///////// Decode CMD_SET_RTC
                            uint8_t day, month, hr, min, sec;
                            uint16_t year;
                            
                            decode_downlink_cmd_set_rtc(rx_data, &day, &month, &year, &hr, &min, &sec);
                            if (year > 2040)
                            {
                                activation_successful = FALSE;
                                DebugLn("[FAIL]INVLD_CMD");
                            }
                            else
                            {

                                ///////// Send ACK_BY_SENSOR (multiple times)
                                send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                                delay(200);
                                send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                                delay(200);
                                send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                                DebugLn("[RCVD]CMDSETRTC");
                                DebugLn("[SENT]ACKBYSENS");

                                ///////// Set RTC
                                MCP7940N_Open();
                                MCP7940N_SetTime(hr, min, sec);
                                delay(1);
                                uint8_t yr_from_2000 = (uint8_t)(year - 2000);
                                MCP7940N_SetDate(1, day, month, yr_from_2000);
                                MCP7940N_Close();

                                ///////// Dump Time Settings
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
                                DebugLn("=ACT.COMPL=");
                                state_transition(MODE_PRE_HIGH_TEMP);
                                RFM69_close();
                                return;
                            }
                        }
                        timeout++;
                    }
                }
            }
            RFM69_close();
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
