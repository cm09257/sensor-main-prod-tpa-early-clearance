// mode_high_temperature.c

#include "modes/mode_high_temperature.h"
#include "modules/storage_internal.h"
#include "utility/debug.h"
#include "app/state_machine.h"
#include "modules/settings.h"
#include "modules/storage.h"
#include "modules/rtc.h"
#include "periphery/power.h"
#include "periphery/tmp126.h"

void mode_high_temperature_run(void)
{
    DebugLn("=== MODE_HIGH_TEMPERATURE START ===");
    DebugLn("[HTEMP] Starte Abkühlkurven-Aufzeichnung...");

    while (state_get_current() == MODE_HIGH_TEMPERATURE)
    {
        // a) Temperatur messen
        TMP126_OpenForMeasurement();
        float temp = TMP126_ReadTemperatureCelsius();
        TMP126_CloseForMeasurement();
        DebugVal("[HTEMP] Temperaturmessung: ", (int)(temp * 100), " x0.01°C");

        // b) Datensatz intern speichern
        record_t rec;
        rec.timestamp = rtc_get_timestamp(); // 5-min Ticks
        rec.temperature = temp;
        rec.flags = 0x01; // gültiger Messwert

        if (!internal_flash_write_record(&rec))
        {
            DebugLn("[HTEMP] Fehler beim Schreiben in internen Flash!");
        }
        else
        {
            DebugLn("[HTEMP] Datensatz gespeichert");
        }

        // c) Prüfen auf Unterschreiten der Abkühl-Schwelle
        float threshold = settings_get()->cool_down_threshold;
        DebugVal("[HTEMP] Schwelle: ", (int)(threshold * 100), " x0.01°C");

        if (temp < threshold)
        {
            DebugLn("[HTEMP] Schwelle unterschritten → Datenübernahme und Moduswechsel");

            copy_internal_to_external_flash();
            DebugLn("[HTEMP] Daten in externen Flash kopiert");

            state_transition(MODE_OPERATIONAL);
            break;
        }

        // d) Nächste Messung planen
        uint8_t interval_min = settings_get()->high_temp_measurement_interval_5min * 5;
        DebugVal("[HTEMP] Nächster Wakeup in ", interval_min, " Minuten");

        rtc_set_alarm_in_minutes(RTC_ALARM_0, interval_min);

        // e) Schlafmodus
        DebugLn("[HTEMP] HALT bis nächster RTC-Alarm");
        mode_before_halt = MODE_HIGH_TEMPERATURE;
        power_enter_halt();
    }
}
