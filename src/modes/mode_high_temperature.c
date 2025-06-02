// mode_high_temperature.c

#include "stm8s_exti.h"
#include "stm8s_gpio.h"
#include "modes/mode_high_temperature.h"
#include "modules/storage_internal.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "app/state_machine.h"
#include "modules/settings.h"
#include "modules/storage.h"
#include "modules/rtc.h"
#include "periphery/power.h"
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/hardware_resources.h"

#define DEBUG_MODE_HI_TEMP 1
volatile bool mode_hi_temp_measurement_alert_triggered = FALSE;

void mode_high_temperature_run(void)
{
    DebugLn("=== MODE_HIGH_TEMPERATURE START ===");
    settings_set_cool_down_threshold(24.0f);

    float threshold = settings_get()->cool_down_threshold;
    uint8_t interval_min = settings_get()->high_temp_measurement_interval_5min * 5;

   // DebugLn("[MODE_HI_TEMP] Settings loaded");
   // DebugFVal("[MODE_HI_TEMP] Low temp threshold = ", threshold, "degC");
   // DebugUVal("[MODE_HI_TEMP] Temperature measurement interval = ", interval_min, "min");
  //  DebugLn("");
  //  DebugLn("[MODE_HI_TEMP] Start measurement of cool-down phase ...");

    // Configuring RTC EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    while (state_get_current() == MODE_HIGH_TEMPERATURE)
    {
        // Measure Temperature
        TMP126_OpenForMeasurement();
        float temp = TMP126_ReadTemperatureCelsius();
        TMP126_CloseForMeasurement();
        DebugFVal("[MODE_HI_TEMP] Temp readout = : ", temp, "degC");

        // Create Data Record
        record_t rec;
        rec.timestamp = rtc_get_timestamp(); // 5-min Ticks
        rec.temperature = temp;
        rec.flags = 0x01; // gültiger Messwert
                          // DebugUVal("[MODE_HI_TEMP] rec.timestamp   = ", rec.timestamp, "x5 min");
                          //  DebugUVal("[MODE_HI_TEMP] rec.temperature = ", rec.temperature, "degC");
                          // DebugUVal("[MODE_HI_TEMP] rec.flags       = ", rec.flags, "(1 = ok)");

        // Save Data Record to Internal Flash
        if (!internal_flash_write_record(&rec))
        {
            DebugLn("[MODE_HI_TEMP] Error writing internal Flash!");
        }
        else
        {
#if defined(DEBUG_MODE_HI_TEMP)
          //  DebugLn("[MODE_HI_TEMP] Saved record to internal flash");
            record_t validation_read_record;
            if (internal_flash_read_record(0, &validation_read_record))
            {
              //  DebugLn("[MODE_HI_TEMP] Re-Read from internal flash successful");
              //  DebugUVal("[MODE_HI_TEMP] Validation timestamp = ", validation_read_record.timestamp, "x 5min");
              //  DebugUVal("[MODE_HI_TEMP] Validation temperature = ", validation_read_record.temperature, "degC");
               // DebugUVal("[MODE_HI_TEMP] Validation data ok flag = ", validation_read_record.flags, "(1=ok)");
            }
#endif
        }

        // Check if temperature is below threshold
        if (temp < threshold)
        {
            DebugLn("[MODE_HI_TEMP] Temperature below threshold -> Copy data and change mode");

            copy_internal_to_external_flash();
            DebugLn("[MODE_HI_TEMP] Copied data from internal to external flash");

            state_transition(MODE_OPERATIONAL);
            return;
        }

        // Plan next temperature measurement using RTC alert

#if defined(DEBUG_MODE_HI_TEMP)
        uint8_t h, m, s;
        rtc_get_time(&h, &m, &s);
        DebugUVal("Current m = ", m, "");
        DebugUVal("Current s", s, "");
        rtc_set_alarm_in_minutes(RTC_ALARM_1, 1);
        delay(1000);
#else
        DebugUVal("[MODE_HI_TEMP] Next Wakeup in ", interval_min, " Minuten");
        rtc_set_alarm_in_minutes(RTC_ALARM_1, interval_min);
#endif
        mode_before_halt = MODE_HIGH_TEMPERATURE;
        enableInterrupts();

        // e) Schlafmodus
        DebugLn("[MODE_HI_TEMP] HALT until next RTC-Alarm");

        // Warten auf Alert (Polling-Variante, falls HALT noch nicht aktiv)
        // TODO: Replace by power_enter_halt(); once completed
        while (!mode_hi_temp_measurement_alert_triggered)
        {
            nop();
        }
        MCP7940N_DisableAlarmX(1);   // immediately disable and clear alarm
        MCP7940N_ClearAlarmFlagX(1); // in ISR
        DebugLn("[MODE_HI_TEMP] Restarting main loop");
        mode_hi_temp_measurement_alert_triggered = FALSE;
    }
}
