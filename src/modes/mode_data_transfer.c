#include "stm8s.h"
#include "app/state_machine.h"
#include "modules/settings.h"
#include "modules/storage.h"
#include "modules/rtc.h"
#include "modules/packet_handler.h"
#include "periphery/mcp7940n.h"
#include "periphery/RFM69.h"
#include "periphery/flash.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include <string.h>


void mode_data_transfer_run(void)
{
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugLn("=MD_DATA_XFER=");
#endif
    ///////////// Determine number of records to transfer
    settings_load();
    settings_t *settings = settings_get();
    uint32_t number_of_records = settings->flash_record_count;
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugUVal("[DTXFR]rec's:", number_of_records, "");
#endif

    //////////////// Send Ping for Data Transfer
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugLn("[SENT]DtXfrPng");
#endif

    //////////// Init retry counter, ack & cmd_announce flags
    uint8_t ping_retry_count = 0;
    bool ping_ack_received = FALSE;
    bool cmd_announced = FALSE;
    bool ping_ack_and_rtc_set_successful = FALSE;

    //////////// Send ping & wait-for-ack loop
    while ((!ping_ack_received) && (ping_retry_count < MAX_DT_XFER_PING_SEND_RETRIES))
    {
        //////// Send ping for data transfer
        RFM69_open();
        send_uplink_ping_for_data_transfer(DEVICE_ID_MSB, DEVICE_ID_LSB, number_of_records);

        //////// Check for ack
        cmd_announced = FALSE;
        ping_ack_received = wait_for_ack_by_gateway(DT_XFER_ACK_TIMEOUT, &cmd_announced);

        if (ping_ack_received)
        {
#if defined(DEBUG_MODE_DATA_TRANSFER)
            DebugLn("[RCVD]AckByGtwy");
#endif
            if (cmd_announced)
            {
                // DebugLn("Wt fr cmd");
                uint8_t rx_data[8];
                RFM69_WriteReg(RFM_REG_IRQ_FLAGS2, 0x10); // FIFOReset
                RFM69_SetModeRx();                        // neuer Sync

                /////// RECEIVE LOOP Wait For Command
                uint16_t timeout = 0;
                while (timeout < TIMEOUT_DT_XFER_WAIT_FOR_CMD)
                {
                    ///////////// Receive packet
                    bool ok = RFM69_ReceiveFixed8BytesECC(rx_data, DT_XFER_CMD_TIMEOUT);

                    ///////////// Extract header/cmd
                    uint8_t header_byte = rx_data[0];
                    uint8_t cmd_byte = rx_data[1];

                    ///////////// CMD_SET_RTC_OFFSET?
                    if (ok && ((header_byte == DOWNLINK_HEADER) && (cmd_byte == CMD_SET_RTC_OFFSET)))
                    {
                        ///////// Decode CMD_SET_RTC
                        uint8_t day, month, hr, min, sec;
                        uint16_t year;

                        decode_downlink_cmd_set_rtc(rx_data, &day, &month, &year, &hr, &min, &sec);
                        if (year > 2040)
                        {
                            DebugLn("[FAIL]InvldCmd");
                        }
                        else
                        {
                            ///////// Send ACK_BY_SENSOR (multiple times)
                            send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                            delay(200);
                            send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                            delay(200);
                            send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                            DebugLn("[RCVD]CmdSetRtc");
                            DebugLn("[SENT]AckBySns");

                            ///////// Set RTC
                            MCP7940N_Open();
                            MCP7940N_SetTime(hr, min, sec);
                            delay(1);
                            uint8_t yr_from_2000 = (uint8_t)(year - 2000);
                            MCP7940N_SetDate(1, day, month, yr_from_2000);
                            MCP7940N_Close();

                            ///////// Dump Time Settings
#if defined(DEBUG_MODE_DATA_TRANSFER)
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
                            DebugLn("=RtcSetCompl=");
                            ping_ack_and_rtc_set_successful = TRUE;
                            RFM69_close();
                        }
                    }
                    timeout++;
                }
            }
        }
        RFM69_close();
        ping_retry_count++;
        delay(DT_XFER_PING_DELAY_BEFORE_RETRY); /// Delay between pings in one try
    }

    //////////////////// Ping and RTC set ok? --> Data Transfer
    if (ping_ack_and_rtc_set_successful)
    {
        //////////////// Open RFM and Flash Modules
        RFM69_open();
        Flash_Open();

        //////////////// Data transfer main loop
        for (uint32_t i = 0; i < number_of_records; i++)
        {
            //////////// Get Flash record
            record_t rec;
            uint32_t address = flash_get_record_address(i);
            uint8_t tmp[sizeof(record_t)];
            Flash_ReadData(address, tmp, sizeof(record_t));
            memcpy(&rec, tmp, sizeof(record_t));

            //////////// Send Data Packet, wait for ack loop evt. resend
            uint8_t dt_packet_retry_count = 0;
            bool dt_packet_ack_received = FALSE;

            //////////// Send/receive loop
            while ((!dt_packet_ack_received) && (dt_packet_retry_count < DT_XFER_MAX_DT_PACKET_SEND_RETRIES))
            {
                send_uplink_data_packet(DEVICE_ID_MSB, DEVICE_ID_LSB, rec.temperature, rec.timestamp);

                //////// Check for ack
                cmd_announced = FALSE;
                dt_packet_ack_received = wait_for_ack_by_gateway(DT_XFER_ACK_TIMEOUT, &cmd_announced);
                if (dt_packet_ack_received)
                {
#if defined(DEBUG_MODE_DATA_TRANSFER)
                    DebugUVal("[DTXFR]rec.", i, " snt");
#endif
                    nop();
                }
                else
                {
                    dt_packet_retry_count++;
                }
            }
        }

        /////////////// Close Flash and RFM
        Flash_Close();
        RFM69_close();
    }
}