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
    uint32_t num_records = settings_get()->flash_record_count;
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugULong("[DTXFR]rec's:", num_records, "");
#endif

    //////////////// Send Ping for Data Transfer
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugLn("[SENT]DtXfrPng");
#endif

    //////////////Open RFM
    RFM69_open();

    //////////// Init retry counter, ack & cmd_announce flags
    uint8_t ping_retry = 0;
    bool ping_ack_ok = FALSE;
    bool cmd_follows = FALSE;
    bool rtc_success = FALSE;

    //////////// Send ping & wait-for-ack loop
    while (!ping_ack_ok && ping_retry < MAX_DT_XFER_PING_SEND_RETRIES)
    {
        //////// Send ping for data transfer
        send_uplink_ping_for_data_transfer(DEVICE_ID_MSB, DEVICE_ID_LSB, num_records);

        //////// Check for ack
        ping_ack_ok = wait_for_ack_by_gateway(DT_XFER_ACK_TIMEOUT, &cmd_follows);

        if (!ping_ack_ok)
        {
            ping_retry++;
            delay(DT_XFER_PING_DELAY_BEFORE_RETRY);
            continue;
        }

#if defined(DEBUG_MODE_DATA_TRANSFER)
        DebugLn("[RCVD]AckByGtwy");
#endif
        if (!cmd_follows)
        {
             rtc_success = TRUE; // If not TRUE -> no Data Transfer will happen
            break; // Gateway will not send RTC command
        }

        /////// Wait for Set_RTC
        uint8_t rx_data[8];
        RFM69_WriteReg(RFM_REG_IRQ_FLAGS2, 0x10); // FIFOReset
        RFM69_SetModeRx();                        // neuer Sync

        /////// RECEIVE LOOP Wait For Command
        for (uint16_t t = 0; t < TIMEOUT_DT_XFER_WAIT_FOR_CMD; ++t)
        {
            ///////////// Receive packet
            if (!RFM69_ReceiveFixed8BytesECC(rx_data, DT_XFER_CMD_TIMEOUT))
                continue;

            ///////////// CMD_SET_RTC_OFFSET?
            if (rx_data[0] != DOWNLINK_HEADER || rx_data[1] != CMD_SET_RTC_OFFSET)
                continue;

            ///////// Decode CMD_SET_RTC
            uint8_t day, month, hr, min, sec;
            uint16_t year;
            decode_downlink_cmd_set_rtc(rx_data, &day, &month, &year, &hr, &min, &sec);
            if (year > 2040)
            {
                DebugLn("[FAIL]InvldCmd");
                break;
            }

            ///////// Send ACK_BY_SENSOR (multiple times)
            for (uint8_t i = 0; i < 3; i++)
            {
                send_uplink_ack_by_sensor(DEVICE_ID_MSB, DEVICE_ID_LSB);
                delay(200);
            }
#if defined(DEBUG_MODE_DATA_TRANSFER)
            DebugLn("[RCVD] CmdSetRtc");
            DebugLn("[SENT] AckBySensor");
#endif

            ///////// Set RTC
            MCP7940N_Open();
            MCP7940N_SetTime(hr, min, sec);
            MCP7940N_SetDate(1, day, month, (uint8_t)(year - 2000));
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
            rtc_success = TRUE;
            break;
        }
        break;
    }

    //////////////////// Ping and RTC set ok? --> Data Transfer
    if (rtc_success)
    {
        //////////////// Open Flash Module
        Flash_Open();

        //////////////// Data transfer main loop
        for (uint32_t idx = 0; idx < num_records; idx++)
        {
            //////////// Get Flash record
            record_t rec;
            uint32_t address = flash_get_record_address(idx);
            uint8_t tmp[sizeof(record_t)];
            Flash_ReadData(address, tmp, sizeof(record_t));
            memcpy(&rec, tmp, sizeof(record_t));

            //////////// Send Data Packet, wait for ack loop evt. resend
            uint8_t retries = 0;
            bool pkt_ack = FALSE;

            //////////// Send/receive loop
            while (!pkt_ack && retries < DT_XFER_MAX_DT_PACKET_SEND_RETRIES)
            {
                send_uplink_data_packet(DEVICE_ID_MSB, DEVICE_ID_LSB, rec.temperature, rec.timestamp);

                //////// Check for ack
                pkt_ack = wait_for_ack_by_gateway(DT_XFER_ACK_TIMEOUT, &cmd_follows);
                if (!pkt_ack)
                    retries++;

#if defined(DEBUG_MODE_DATA_TRANSFER)
                if (pkt_ack)
                    DebugULong("[DTXFR]rec.", idx, " snt");
                else
                    DebugULong("[DTXFR]rec.", idx, " err");
#endif
            }

            /////////////// Close Flash and RFM
        }
        Flash_Close();
    }
    RFM69_close();
    state_transition(MODE_OPERATIONAL);
    return;
}