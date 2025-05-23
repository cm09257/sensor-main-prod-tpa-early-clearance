#include "modules/radio.h"
#include "modules/uplink_builder.h"
#include "modules/settings.h"
#include "periphery/rfm69.h"
#include "config/config.h"
#include "utility/delay.h"
#include "utility/debug.h"
#include "utility/random.h"
#include "app/state_machine.h"
#include <string.h>

static radio_mode_t current_radio_mode = RADIO_MODE_SLEEP;

void radio_init(void)
{
    DebugLn("[RADIO] Initialisiere Funkmodul...");

    RFM69_GPIO_Init_All();
    RFM69_Init();
    RFM69_SetPA(TRUE);
    RFM69_SetSyncWord(0x2D, 0xD4);
    RFM69_SetModeStandby();
    current_radio_mode = RADIO_MODE_STANDBY;

    DebugLn("[RADIO] Init abgeschlossen, im STANDBY-Modus.");
}

void radio_set_mode(radio_mode_t mode)
{
    if (mode == current_radio_mode) return;

    Debug("[RADIO] Wechsel zu Modus: ");
    switch (mode) {
        case RADIO_MODE_SLEEP:   DebugLn("SLEEP");   RFM69_SetModeSleep();   break;
        case RADIO_MODE_STANDBY: DebugLn("STANDBY"); RFM69_SetModeStandby(); break;
        case RADIO_MODE_RX:      DebugLn("RX");      RFM69_SetModeRx();      break;
        case RADIO_MODE_TX:      DebugLn("TX");      RFM69_SetModeTx();      break;
    }

    current_radio_mode = mode;
}

radio_result_t radio_send_packet(const uint8_t* data, uint8_t len)
{
    DebugVal("[RADIO] Sende Paket mit Länge", len, " Bytes");

    radio_set_mode(RADIO_MODE_TX);
    bool ok = RFM69_Send(data, len, 200);
    if (!ok) {
        DebugLn("[RADIO] Senden fehlgeschlagen.");
        return RADIO_ERROR;
    }

    radio_set_mode(RADIO_MODE_RX);
    uint8_t rx_buf[8] = {0};
    uint8_t rx_len = 0;
    if (RFM69_Receive(rx_buf, &rx_len, 100)) {
        DebugVal("[RADIO] Empfangsantwort mit Länge", rx_len, " Bytes");
        if (rx_len > 0) {
            if (rx_buf[0] == RADIO_ACK_CODE) {
                DebugLn("[RADIO] ACK empfangen.");
                return RADIO_ACK_RECEIVED;
            } else if (rx_buf[0] == RADIO_NACK_CODE) {
                DebugLn("[RADIO] NACK empfangen.");
                return RADIO_NACK_RECEIVED;
            }
        }
    } else {
        DebugLn("[RADIO] Keine Antwort empfangen.");
    }

    return RADIO_NO_RESPONSE;
}

radio_result_t radio_send_data(const record_t* records, uint8_t count, const uint8_t* device_id, uint8_t seq_nr)
{
    uint8_t packet[MAX_UPLINK_PACKET_SIZE];
    uint8_t len = uplink_build_packet(records, count, device_id, seq_nr, packet);

    DebugVal("[RADIO] Sende Datenuplink, SeqNr:", seq_nr, "");

    for (uint8_t attempt = 0; attempt < MAX_RADIO_ATTEMPTS; ++attempt) {
        DebugVal("→ Sende Versuch", attempt + 1, "");
        if (radio_send_packet(packet, len) == RADIO_ACK_RECEIVED) {
            return RADIO_ACK_RECEIVED;
        }
        uint16_t delay_ms = random16() % MAX_RESEND_DELAY_MS;
        DebugVal("→ Wartezeit vor erneutem Versuch", delay_ms, " ms");
        delay(delay_ms);
    }

    DebugLn("[RADIO] Kein ACK nach mehreren Versuchen.");
    return RADIO_NO_RESPONSE;
}

radio_result_t radio_send_ping(uint8_t ping_type)
{
    uint8_t ping_packet[2] = { ping_type, 0x00 };

    DebugHex("[RADIO] Sende Ping mit Typ", ping_type);

    for (uint8_t attempt = 0; attempt < MAX_PING_RETRIES; ++attempt)
    {
        DebugVal("→ Ping-Versuch", attempt + 1, "");
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

bool radio_receive_for_activation(void)
{
    DebugLn("[RADIO] Warte auf Aktivierungsbefehl...");

    uint8_t buffer[16];
    uint8_t length = sizeof(buffer);

    if (!RFM69_Receive(buffer, &length, 1000)) {
        DebugLn("Kein Paket empfangen.");
        return FALSE;
    }

    DebugVal("Empfangenes Paket, Länge: ", length, " Bytes");

    if (length < 7) {
        DebugLn("Paket zu kurz → NACK");
        radio_send_nack();
        return FALSE;
    }

    if (buffer[0] != 0xB0 || buffer[6] != CMD_ACTIVATION) {
        DebugLn("Ungültiger Header oder Befehl → NACK");
        radio_send_nack();
        return FALSE;
    }

    const settings_t* settings = settings_get();
    uint32_t our_id = settings->device_id;
    uint32_t rx_id  = *(uint32_t*)&buffer[2];

    if (our_id != rx_id) {
        DebugLn("Falsche Geräte-ID → NACK");
        radio_send_nack();
        return FALSE;
    }

    DebugLn("Aktivierungsbefehl empfangen → ACK, Moduswechsel.");
    radio_send_ack();
    state_transition(MODE_HIGH_TEMPERATURE);
    return TRUE;
}

void radio_send_ack(void) {
    DebugLn("[RADIO] Sende ACK");
    const uint8_t ack[] = {0xAA};
    RFM69_Send(ack, sizeof(ack), 1000);
}

void radio_send_nack(void) {
    DebugLn("[RADIO] Sende NACK");
    const uint8_t nack[] = {0x55};
    RFM69_Send(nack, sizeof(nack), 1000);
}

bool radio_receive_downlink(uint8_t* cmd, uint8_t* payload, uint8_t* length)
{
    uint8_t buffer[16];
    uint8_t len = sizeof(buffer);

    if (!RFM69_Receive(buffer, &len, 300)) {
        DebugLn("[RADIO] Kein Downlink empfangen.");
        return FALSE;
    }

    if (len < 2 || buffer[0] != RADIO_DOWNLINK_HEADER) {
        DebugLn("[RADIO] Ungültiger Downlink-Header.");
        return FALSE;
    }

    *cmd = buffer[1];
    *length = len - 2;
    memcpy(payload, &buffer[2], *length);

    DebugVal("[RADIO] Downlink CMD:", *cmd, "");
    DebugVal("[RADIO] Payload-Länge:", *length, "");

    return TRUE;
}
