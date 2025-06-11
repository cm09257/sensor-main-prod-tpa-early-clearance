#include "stm8s.h"
#include "app/state_machine.h"
#include "modules/settings.h"
//#include "modules/radio.h"
#include "modules/storage.h"
#include "modules/rtc.h"
#include "periphery/mcp7940n.h"
#include "utility/debug.h"

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

void mode_data_transfer_run(void)
{
    DebugLn("=== MODE_DATA_TRANSFER START ===");

    if (!perform_handshake_and_timesync()) {
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

    while (index < total)
    {
        if (!send_next_packet(cfg, &index, &seq_nr, total)) {
            success = FALSE;
            break;
        }
    }

    DebugLn(success ? "[DT] Alle Datensätze erfolgreich übertragen" : "[DT] Übertragung unvollständig");
    DebugLn("=== MODE_DATA_TRANSFER ENDE ===");
    state_transition(MODE_SLEEP);
}
*/
void mode_data_transfer_run(void)
{
    nop();
}