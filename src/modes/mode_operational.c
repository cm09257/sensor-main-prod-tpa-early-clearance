#include "app/state_machine.h"
#include "common/types.h"
#include "modes/mode_operational.h"
#include "modules/radio.h"
#include "modules/settings.h"
#include "modules/storage.h"
#include "periphery/tmp126.h"
#include "utility/debug.h"

// Optional: Definition der Flags für record_t.flags
#define FLAG_NONE       0x00
#define FLAG_SENSOR_ERR 0x02

static uint8_t wakeup_counter = 0; // zählt Wakeups nach Messintervall

void mode_operational_run(void)
{
    DebugLn("=== MODE_OPERATIONAL START ===");

    TMP126_OpenForMeasurement();
    float temp_c = TMP126_ReadTemperatureCelsius();
    TMP126_CloseForMeasurement();

    if (temp_c < -100.0f || temp_c > 200.0f)
    {
        DebugLn("[OPERATIONAL] Sensorfehler oder ungültiger Messwert");
        state_transition(MODE_SLEEP);
        return;
    }

    DebugVal("[OPERATIONAL] Gemessene Temperatur: ", (int)(temp_c * 100), " x0.01°C");

    timestamp_t ts = rtc_get_timestamp();
    Debug("[OPERATIONAL] Zeitstempel: ");
    char ts_str[12];
    sprintf(ts_str, "%lu", (unsigned long)ts);
    DebugLn(ts_str);

    record_t rec;
    rec.timestamp = ts;
    rec.temperature = temp_c;
    rec.flags = FLAG_NONE;

    if (!flash_write_record(&rec))
    {
        DebugLn("[OPERATIONAL] Fehler beim Schreiben in den Flash-Speicher");
        state_transition(MODE_SLEEP);
        return;
    }

    DebugLn("[OPERATIONAL] Datensatz erfolgreich im Flash gespeichert");

    // Zähler erhöhen
    wakeup_counter++;
    DebugVal("[OPERATIONAL] Wakeup-Zähler: ", wakeup_counter, "");

    // Konfiguration abrufen
    const settings_t *cfg = settings_get();
    DebugVal("[OPERATIONAL] Sendefaktor: ", cfg->send_interval_factor, "");

    if ((wakeup_counter % cfg->send_interval_factor) == 0)
    {
        DebugLn("[OPERATIONAL] Funkintervall erreicht → Wechsel in MODE_DATA_TRANSFER");
        wakeup_counter = 0;
        state_transition(MODE_DATA_TRANSFER);
    }
    else
    {
        DebugLn("[OPERATIONAL] Funkintervall noch nicht erreicht → Wechsel in MODE_SLEEP");
        state_transition(MODE_SLEEP);
    }
}
