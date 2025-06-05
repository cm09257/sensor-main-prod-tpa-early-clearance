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
#include "periphery/uart.h"
#include "periphery/mcp7940n.h"
#include "periphery/hardware_resources.h"

#define DEBUG_MODE_HI_TEMP 1
#define DEV_HI_TEMP_SKIP_AFTER 3
#define HI_TEMP_RAM_BUFFER_SIZE 72

volatile bool mode_hi_temp_measurement_alert_triggered = FALSE;
static uint8_t dev_hi_temp_counter = 0;
static record_t hi_temp_buffer[HI_TEMP_RAM_BUFFER_SIZE];
static uint8_t hi_temp_buffer_index = 0;

void mode_high_temperature_run(void)
{
    DebugLn("=== MODE_HIGH_TEMPERATURE START ===");
    settings_set_cool_down_threshold(21.0f);

    ///// Loading settings
    float threshold = settings_get()->cool_down_threshold;
    uint8_t interval_min = settings_get()->high_temp_measurement_interval_5min * 5;

    // DebugLn("[MODE_HI_TEMP] Settings loaded");
    DebugFVal("[MODE_HI_TEMP] Low temp threshold = ", threshold, "degC");
    // DebugUVal("[MODE_HI_TEMP] Temperature measurement interval = ", interval_min, "min");
    // DebugLn("");
    // DebugLn("[MODE_HI_TEMP] Start measurement of cool-down phase ...");

    ///// Configuring RTC EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    ///// Main loop

    while (state_get_current() == MODE_HIGH_TEMPERATURE)
    {
        ///// Measure Temperature
        TMP126_OpenForMeasurement();
        float temp = TMP126_ReadTemperatureCelsius();
        TMP126_CloseForMeasurement();
        DebugFVal("[MODE_HI_TEMP] Temp readout = : ", temp, "degC");

        ///// Creating data record
        record_t rec;
        rec.timestamp = rtc_get_timestamp(); // 5-min Ticks
        rec.temperature = temp;
        rec.flags = 0x01; // gültiger Messwert
                          // DebugUVal("[MODE_HI_TEMP] rec.timestamp   = ", rec.timestamp, "x5 min");
                          //  DebugUVal("[MODE_HI_TEMP] rec.temperature = ", rec.temperature, "degC");
                          // DebugUVal("[MODE_HI_TEMP] rec.flags       = ", rec.flags, "(1 = ok)");

        ///// Store data record in RAM (array)
        if (hi_temp_buffer_index < HI_TEMP_RAM_BUFFER_SIZE)
        {
            hi_temp_buffer[hi_temp_buffer_index++] = rec;
        }
        else
        {
            DebugLn("[MODE_HI_TEMP] RAM data buffer full!");
        }

        ///// Development phase: After three measurements --> Copy data and transition to MODE_OPERATIONAL
#if defined(DEBUG_MODE_HI_TEMP)
        dev_hi_temp_counter++;
        DebugUVal("[DEV] HighTemp Measurement Count: ", dev_hi_temp_counter, "");
        if (dev_hi_temp_counter >= DEV_HI_TEMP_SKIP_AFTER)
        {
            DebugLn("[DEV] Threshold ignored -> Go to Copy & Change Mode");
            temp = threshold - 1; // Schwellenwert künstlich unterschreiten
        }
#endif

        ///// Check if temperature is below threshold
        if (temp < threshold)
        {
            ///// Copy data from RAM --> Ext. Flash
            DebugLn("[MODE_HI_TEMP] Temperature below threshold -> Copy data and change mode");
            for (uint8_t i = 0; i < hi_temp_buffer_index; i++)
            {
                if (!flash_write_record_nolock(&hi_temp_buffer[i]))
                {
                    DebugUVal("[MODE_HI_TEMP] Error writing external flash for record with index ", i, "");
                    break;
                }
            }
            DebugLn("[MODE_HI_TEMP] RAM-Data copied to external flash");
            hi_temp_buffer_index = 0;

            // DebugLn("[DEBUG] Lese erste 3 Datensaetze aus externem Flash...");

            record_t rec;
            flash_read_record(0, &rec);
            uint16_t ts = rec.timestamp;
            float tm = rec.temperature;
            uint8_t fl = rec.flags;

            char buf[32];
            sprintf(buf, "record temp = %u", (uint8_t)tm);
            DebugLn(buf);
            // DebugLn("Index 0");
            // DebugUVal("  Timestamp = ", ts, " x5min");
            // DebugFVal("  Temperature = ", tm, "degC");
            // DebugUVal("  Flag = ", fl, " (1 = ok)");

            state_transition(MODE_OPERATIONAL);
            return;
        }

        ///// Plan next temperature measurement using RTC alert
#if defined(DEBUG_MODE_HI_TEMP)
        char buf[32];
        rtc_get_format_time(buf);
        DebugLn(buf);

        rtc_set_alarm_in_minutes(RTC_ALARM_1, 1);
        delay(1000);
#else
        DebugUVal("[MODE_HI_TEMP] Next Wakeup in ", interval_min, " Minuten");
        rtc_set_alarm_in_minutes(RTC_ALARM_1, interval_min);
#endif
        mode_before_halt = MODE_HIGH_TEMPERATURE;

        ///// Go to power_halt mode, wakeup using RTC EXTI
        DebugLn("[MODE_HI_TEMP] HALT until next RTC-Alarm");
        power_enter_halt();
        delay(100);
        enableInterrupts();
        __asm__("halt");

        ///// After HALT: RTC-Interrupt was triggered
        rtc_clear_and_disable_alarm(RTC_ALARM_1);

        //  DebugLn("[MODE_HI_TEMP] Restarting main loop");
        mode_hi_temp_measurement_alert_triggered = FALSE;
    }
}
