#include "stm8s.h"
#include "app/state_machine.h"
#include "modules/settings.h"
// #include "modules/radio.h"
#include "modules/storage.h"
#include "modules/rtc.h"
#include "periphery/mcp7940n.h"
#include "periphery/RFM69.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include <string.h>

#define RADIO_TX_TIMEOUT_MS 200
#define RADIO_RX_TIMEOUT_MS 100
#define RADIO_ACTIVATION_TIMEOUT_MS 1000
#define RADIO_ACK_TIMEOUT_MS 1000
#define RADIO_RX_DOWNLINK_TIMEOUT 300

typedef enum
{
    RADIO_ACK_RECEIVED,  ///< Übertragung erfolgreich (ACK empfangen)
    RADIO_NACK_RECEIVED, ///< Übertragung abgelehnt (NACK empfangen)
    RADIO_NO_RESPONSE,   ///< Keine Antwort empfangen
    RADIO_ERROR          ///< Fehler beim Senden oder Empfangen
} radio_result_t;

/*
static bool perform_handshake_and_timesync(void)
{
    DebugLn("[DT] Sende Data-Ping zur Initialisierung...");
    if (radio_send_ping(RADIO_PING_HEADER_DATA_TRANSFER) != RADIO_ACK_RECEIVED)
    {
        DebugLn("[DT] Kein ACK → Abbruch");
        return FALSE;
    }
    DebugLn("[DT] ACK erhalten → Transfer beginnt");

    uint8_t cmd;
    uint8_t payload[8];
    uint8_t len = 0;
    if (radio_receive_downlink(&cmd, payload, &len) && cmd == CMD_SET_RTC_OFFSET && len == 4) {
        uint32_t ts = *(uint32_t*)payload;
        rtc_set_unix_timestamp(ts);
        DebugUVal("[DT] RTC synchronisiert auf ", ts, " Sekunden seit 1970");
    }
    return TRUE;
}



static bool send_next_packet(const settings_t* cfg, uint16_t* index, uint8_t* seq_nr, uint16_t total)
{
    record_t buffer[MAX_RECORDS_PER_PACKET];
    uint8_t count = 0;

    while (count < MAX_RECORDS_PER_PACKET && *index < total)
    {
        if (!flash_read_record(*index, &buffer[count]))
        {
            DebugUVal("[DT] Fehler beim Lesen @ Index ", *index, "");
            return FALSE;
        }
        count++;
        (*index)++;
    }

    if (count == 0)
    {
        DebugLn("[DT] Keine Daten geladen -> Abbruch");
        return FALSE;
    }

    DebugUVal("[DT] Sende Paket mit ", count, " Datensätzen");

    radio_result_t result = radio_send_data(buffer, count, (uint8_t*)&cfg->device_id, *seq_nr);
    if (result == RADIO_ACK_RECEIVED)
    {
        DebugLn("[DT] ACK erhalten -> nächstes Paket");
        (*seq_nr)++;
        return TRUE;
    }

    if (result == RADIO_NACK_RECEIVED)
    {
        DebugLn("[DT] NACK erhalten – Wiederholung");

        for (uint8_t attempt = 1; attempt <= MAX_NACK_RETRIES; ++attempt)
        {
            DebugUVal("[DT] NACK-Retry Nr. ", attempt, "");
            result = radio_send_data(buffer, count, (uint8_t*)&cfg->device_id, *seq_nr);

            if (result == RADIO_ACK_RECEIVED)
            {
                DebugLn("[DT] Wiederholung erfolgreich (ACK)");
                (*seq_nr)++;
                return TRUE;
            }
            else if (result == RADIO_NACK_RECEIVED)
            {
                DebugLn("[DT] Erneut NACK erhalten");
            }
            else
            {
                DebugLn("[DT] Keine Antwort oder Fehler beim Retry – Abbruch");
                break;
            }
        }

        DebugLn("[DT] NACK-Retries aufgebraucht – Abbruch");
        return FALSE;
    }

    DebugLn("[DT] Keine Antwort oder Funkfehler – Abbruch");
    return FALSE;
}
*/
/*
static radio_result_t radio_send_packet(const uint8_t *data, uint8_t len)
{
    DebugUVal("[RADIO] Sende Paket mit Länge", len, " Bytes");

    if (!RFM69_open())
    {
        DebugLn("[RADIO] Öffnen des Funkmoduls fehlgeschlagen.");
        return RADIO_ERROR;
    }

    RFM69_SetModeTx();
    bool ok = RFM69_Send(data, len, RADIO_TX_TIMEOUT_MS);
    if (!ok)
    {
        DebugLn("[RADIO] Senden fehlgeschlagen.");
        RFM69_close();
        return RADIO_ERROR;
    }

    RFM69_SetModeRx();
    uint8_t rx_buf[8] = {0};
    uint8_t rx_len = 0;
    bool received = RFM69_Receive(rx_buf, &rx_len, RADIO_RX_TIMEOUT_MS);

    RFM69_close();

    if (received && rx_len > 0)
    {
        if (rx_buf[0] == RADIO_ACK_CODE)
        {
            DebugLn("[RADIO] ACK empfangen.");
            return RADIO_ACK_RECEIVED;
        }
        else if (rx_buf[0] == RADIO_NACK_CODE)
        {
            DebugLn("[RADIO] NACK empfangen.");
            return RADIO_NACK_RECEIVED;
        }
    }
    else
    {
        DebugLn("[RADIO] Keine Antwort empfangen.");
    }

    return RADIO_NO_RESPONSE;
}*/

/*static bool radio_send_raw(const uint8_t *data, uint8_t len)
{
    RFM69_SetModeTx();
    bool ok = RFM69_Send(data, len, RADIO_TX_TIMEOUT_MS);
    RFM69_SetModeRx();
    return ok;
}
*/

bool radio_send_ping_packet(uint8_t ping_type)
{
    uint8_t ping_packet[2] = {ping_type, 0x00};
    DebugHex("[RADIO] Sende Ping mit Typ ", ping_type);

    RFM69_open();
    RFM69_SetModeTx();
    bool ok = RFM69_Send(ping_packet, sizeof(ping_packet), RADIO_TX_TIMEOUT_MS);
    RFM69_SetModeRx();
    RFM69_close();

    if (!ok)
        DebugLn("[RADIO] Senden fehlgeschlagen.");

    return ok;
}
/*static radio_result_t radio_receive_ack_or_nack(void)
{
    uint8_t rx_buf[8];
    uint8_t rx_len = 0;

    if (!RFM69_Receive(rx_buf, &rx_len, RADIO_RX_TIMEOUT_MS) || rx_len == 0)
    {
        DebugLn("[RADIO] Keine Antwort empfangen.");
        return RADIO_NO_RESPONSE;
    }

    if (rx_buf[0] == RADIO_ACK_CODE)
        return RADIO_ACK_RECEIVED;

    if (rx_buf[0] == RADIO_NACK_CODE)
        return RADIO_NACK_RECEIVED;

    DebugUVal("[RADIO] Unerwarteter Antwortcode: ", rx_buf[0], "");
    return RADIO_NO_RESPONSE;
}*/

/*
static radio_result_t radio_send_ping(uint8_t ping_type)
{
    uint8_t ping_packet[2] = {ping_type, 0x00};

    DebugHex("[RADIO] Sending ping with type ", ping_type);

    for (uint8_t attempt = 0; attempt < MAX_PING_RETRIES; ++attempt)
    {
        DebugUVal("-> Ping try number ", attempt + 1, "");
        radio_result_t result = radio_send_packet(ping_packet, sizeof(ping_packet));

        switch (result)
        {
        case RADIO_ACK_RECEIVED:
            DebugLn("Ping -> ACK");
            return RADIO_ACK_RECEIVED;

        case RADIO_NACK_RECEIVED:
            DebugLn("Ping -> NACK (Gateway lehnt ab), retry...");
            delay(50);
            continue;

        case RADIO_NO_RESPONSE:
        case RADIO_ERROR:
        default:
            DebugLn("Ping -> keine Antwort / Fehler, Abbruch");
            return result;
        }
    }

    DebugLn("Alle Ping-Versuche fehlgeschlagen.");
    return RADIO_NACK_RECEIVED;
}
*/
/*
bool radio_receive_downlink(uint8_t *cmd, uint8_t *payload, uint8_t *length)
{
    if (!RFM69_open())
    {
        DebugLn("[RADIO] Öffnen des Funkmoduls fehlgeschlagen.");
        return FALSE;
    }

    uint8_t buffer[16];
    uint8_t len = sizeof(buffer);
    bool received = RFM69_Receive(buffer, &len, RADIO_RX_DOWNLINK_TIMEOUT);

    RFM69_close();

    if (!received)
    {
        DebugLn("[RADIO] Kein Downlink empfangen.");
        return FALSE;
    }

    if (len < 2 || buffer[0] != RADIO_DOWNLINK_HEADER)
    {
        DebugLn("[RADIO] Ungültiger Downlink-Header.");
        return FALSE;
    }

    *cmd = buffer[1];
    *length = len - 2;
    memcpy(payload, &buffer[2], *length);

    DebugUVal("[RADIO] Downlink CMD:", *cmd, "");
    DebugUVal("[RADIO] Payload-Länge:", *length, "");

    return TRUE;
}*/
/*
static bool perform_handshake_and_timesync(void)
{
    DebugLn("[DATA_TRANSFER] Sending data ping for initialization...");
    if (FALSE)//radio_send_ping(RADIO_PING_HEADER_DATA_TRANSFER) != RADIO_ACK_RECEIVED)
    {
        DebugLn("[DATA_TRANSFER] No ACK -> Abort");
        return FALSE;
    }
    DebugLn("[DATA_TRANSFER] ACK received -> Initiating transfer");
/*
    uint8_t cmd;
    uint8_t payload[8];
    uint8_t len = 0;
    if (radio_receive_downlink(&cmd, payload, &len) && cmd == CMD_SET_RTC_OFFSET && len == 4)
    {
        uint32_t ts = *(uint32_t *)payload;
        uint16_t lo_word = (uint16_t)(ts & 0xFFFF);
        uint16_t hi_word = (uint16_t)((ts >> 16) & 0xFFFF);

        DebugHex16("[DATA_TRANSFER] RTC sync timestamp sec since 1970: Low word = ", lo_word);
        DebugHex16("[DATA_TRANSFER]                                    Hi  word = ", hi_word);

        rtc_set_unix_timestamp(ts);
    }

    return TRUE;
}*/

void mode_data_transfer_run(void)
{
    DebugLn("=============== MODE_DATA_TRANSFER START===============");

    ///////////// Determine number of records to transfer
    settings_load();
    settings_t *settings = settings_get();
    uint32_t number_of_records = settings->flash_record_count;
    DebugUVal("[DATA_TRANSFER] Number of flash records: ", number_of_records, "");

    ///////////// Open RFM
    RFM69_open();

    ///////////// Handshake and Time Sync
    bool ok = FALSE;
    uint8_t count = 0;
    while (count < 5)
    {
        DebugLn("[DATA_TRANSFER] Sending data ping for initialization...");
        ok = radio_send_ping_packet(RADIO_PING_HEADER_DATA_TRANSFER);        
        if (ok)
        {
            DebugLn("Ping send ok");
            delay(2000);
            count++;
        }
        else
        {
            DebugLn("Ping send failed");
            count++;
        }
    }
    while (1)
    {
        nop();
    }

    if (FALSE) //! perform_handshake_and_timesync())
    {
        DebugLn("[DATA_TRANSFER] Handshake and sync failed.");
        state_transition(MODE_SLEEP);
        return;
    }

    uint16_t total = flash_get_count();
    DebugUVal("[DT] Anzahl gespeicherter Datensätze: ", total, "");
    if (total == 0)
    {
        DebugLn("[DT] Keine Daten zu übertragen");
        state_transition(MODE_SLEEP);
        return;
    }

    const settings_t *cfg = settings_get();
    DebugHex("[DT] Geräte-ID: ", cfg->device_id);
    DebugLn("[DT] Phase 1: Alle Daten werden gesendet");

    uint16_t index = 0;
    uint8_t seq_nr = 0;
    bool success = TRUE;

    /*
    while (index < total)
    {
        if (!send_next_packet(cfg, &index, &seq_nr, total))
        {
            success = FALSE;
            break;
        }
    }*/
    DebugLn("TODO: IMPLEMENT SENDING OF PACKETS");

    DebugLn(success ? "[DT] Alle Datensätze erfolgreich übertragen" : "[DT] Übertragung unvollständig");
    DebugLn("=== MODE_DATA_TRANSFER ENDE ===");
    while (1)
    {
        nop();
    }

    state_transition(MODE_SLEEP);
}
/*
    void mode_data_transfer_run(void)
{
    nop();
}*/