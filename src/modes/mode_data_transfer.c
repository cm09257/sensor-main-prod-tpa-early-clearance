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

bool radio_send_ping_packet(uint8_t ping_type)
{
    uint8_t ping_packet[2] = {ping_type, 0x00};
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugHex("[RADIO] Sende Ping mit Typ ", ping_type);
#endif

    RFM69_open();
    RFM69_SetModeTx();
    delay(5000);
    bool ok = RFM69_Send(ping_packet, sizeof(ping_packet), RADIO_TX_TIMEOUT_MS);
    RFM69_SetModeStandby();
    RFM69_close();

    if (!ok)
    {
#if defined(DEBUG_MODE_DATA_TRANSFER)
        DebugLn("[RADIO] Senden fehlgeschlagen.");
#endif
        nop();
    }

    return ok;
}

void mode_data_transfer_run(void)
{
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugLn("=============== MODE_DATA_TRANSFER START===============");
#endif
    ///////////// Determine number of records to transfer
    settings_load();
    settings_t *settings = settings_get();
    uint32_t number_of_records = settings->flash_record_count;
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugUVal("[DATA_TRANSFER] Number of flash records: ", number_of_records, "");
#endif
    ///////////// Open RFM

    ///////////// Handshake and Time Sync
    bool ok = FALSE;
    uint8_t count = 0;
    while (count < 5000)
    {
#if defined(DEBUG_MODE_DATA_TRANSFER)
        DebugLn("[DATA_TRANSFER] Sending data ping for initialization...");
#endif
        ok = radio_send_ping_packet(RADIO_PING_HEADER_DATA_TRANSFER);
        if (ok)
        {
#if defined(DEBUG_MODE_DATA_TRANSFER)
            DebugLn("Ping send ok");
#endif
            delay(2000);
            count++;
        }
        else
        {
#if defined(DEBUG_MODE_DATA_TRANSFER)
            DebugLn("Ping send failed");
#endif
            count++;
        }
    }
    while (1)
    {
        nop();
    }

    if (FALSE) //! perform_handshake_and_timesync())
    {
#if defined(DEBUG_MODE_DATA_TRANSFER)
        DebugLn("[DATA_TRANSFER] Handshake and sync failed.");
#endif
        return;
    }

    uint16_t total = flash_get_count();
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugUVal("[DT] Anzahl gespeicherter Datensätze: ", total, "");
#endif
    if (total == 0)
    {
#if defined(DEBUG_MODE_DATA_TRANSFER)
        DebugLn("[DT] Keine Daten zu übertragen");
#endif
        return;
    }

    const settings_t *cfg = settings_get();
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugHex("[DT] Geräte-ID: ", cfg->device_id);
    DebugLn("[DT] Phase 1: Alle Daten werden gesendet");
#endif

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
#if defined(DEBUG_MODE_DATA_TRANSFER)
    DebugLn("TODO: IMPLEMENT SENDING OF PACKETS");

    DebugLn(success ? "[DT] Alle Datensätze erfolgreich übertragen" : "[DT] Übertragung unvollständig");
    DebugLn("=== MODE_DATA_TRANSFER ENDE ===");
#endif
    while (1)
    {
        nop();
    }

}