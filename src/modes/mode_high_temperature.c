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
#include "periphery/flash.h"
#include <string.h>

#define DEBUG_MODE_HI_TEMP 1
#define DEV_HI_TEMP_SKIP_AFTER 3
#define HI_TEMP_RAM_BUFFER_SIZE 72

volatile bool mode_hi_temp_measurement_alert_triggered = FALSE;
static uint8_t dev_hi_temp_counter = 0;
static record_t hi_temp_buffer[HI_TEMP_RAM_BUFFER_SIZE];
static uint8_t hi_temp_buffer_index = 0;

void mode_high_temperature_run(void)
{
    DebugLn("=============== MODE_HIGH_TEMPERATURE START ===============");

    uint32_t address = 0x0100;
    uint8_t test_data[16];
    uint8_t read_back[16];

    // Füllung: 0x00, 0x01, ..., 0x0F
    for (uint8_t i = 0; i < 16; i++) {
        test_data[i] = i;
    }

    Flash_Open();

    DebugHex16("[FLASH TEST] Writing at:", (uint16_t)address);
    Flash_PageProgram(address, test_data, sizeof(test_data));

    DebugLn("[FLASH TEST] Reading back...");
    Flash_ReadData(address, read_back, sizeof(read_back));

    DebugLn("[FLASH TEST] Dump:");
    for (uint8_t i = 0; i < sizeof(read_back); i++) {
        char label[24];
        sprintf(label, "Byte %02u = ", i);
        DebugHex(label, read_back[i]);
    }

    Flash_Close();

    while (1) {nop();}
    



    /// Resetting index
    hi_temp_buffer_index = 0;
    /// Clearing buffer
    memset(hi_temp_buffer, 0, sizeof(hi_temp_buffer));

    ///////////// For debug purposes: setting cool-down threshold // TODO: Remove for production
    settings_set_cool_down_threshold(21.0f);

    ///////////// Loading settings
    float threshold = settings_get()->cool_down_threshold;
    uint8_t interval_min = settings_get()->high_temp_measurement_interval_5min * 5;
    DebugLn("[MODE_HI_TEMP] Settings loaded");
    DebugFVal("[MODE_HI_TEMP] Low temp threshold = ", threshold, "degC");

    ///////////// Configuring RTC_WAKE pin for EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    ///////////// Main loop
    DebugLn("[MODE_HI_TEMP] Starting data acquisition of cool-down phase ...");
    while (state_get_current() == MODE_HIGH_TEMPERATURE)
    {
        ///////////// Measure Temperature
        TMP126_OpenForMeasurement();
        float temp = TMP126_ReadTemperatureCelsius();
        TMP126_CloseForMeasurement();
        DebugFVal("[MODE_HI_TEMP] Readout  Temp = ", temp, "degC");

        ///////////// Creating data record
        record_t rec;
        rec.timestamp = rtc_get_timestamp(); // 5-min Ticks
        rec.temperature = temp;
        rec.flags = 0x01; // 0x01 = valid value, 0x00 =error
                          // DebugUVal("[MODE_HI_TEMP] rec.timestamp   = ", rec.timestamp, "x5 min");
                          //  DebugUVal("[MODE_HI_TEMP] rec.temperature = ", rec.temperature, "degC");
                          // DebugUVal("[MODE_HI_TEMP] rec.flags       = ", rec.flags, "(1 = ok)");

        ///////////// Store data record in RAM (array)
        if (hi_temp_buffer_index < HI_TEMP_RAM_BUFFER_SIZE)
        {
            hi_temp_buffer[hi_temp_buffer_index++] = rec;
        }
        else
        {
            DebugLn("[MODE_HI_TEMP] RAM data buffer full!");
        }

        ///////////// Development phase: After three measurements --> Copy data and transition to MODE_OPERATIONAL // TODO: Remove counter
#if defined(DEBUG_MODE_HI_TEMP)
        dev_hi_temp_counter++;
        DebugUVal("[MODE_HI_TEMP] [DEV] HighTemp Measurement Count: ", dev_hi_temp_counter, "");
        if (dev_hi_temp_counter >= DEV_HI_TEMP_SKIP_AFTER)
        {
            DebugLn("[MODE_HI_TEMP] [DEV] Threshold ignored -> Go to Copy & Change Mode");
            temp = threshold - 1; // Schwellenwert künstlich unterschreiten
        }
#endif

        ///////////// Check if temperature is below threshold
        if (temp < threshold)
        {
            ///////////// Copy data from RAM --> Ext. Flash
            DebugLn("[MODE_HI_TEMP] Temperature below threshold -> Copy data and change mode");

            /// Adress in flash, calculation in for loop.
            uint32_t address;
            uint8_t size_record = sizeof(record_t);

            Flash_Open();
            for (uint16_t i = 0; i < hi_temp_buffer_index; i++)
            {
                /// Compute adress in flash, taking page limits into account.
                address = flash_get_record_address(i);

                /// Serialize record
                uint8_t tmp[sizeof(record_t)];
                memcpy(tmp, &hi_temp_buffer[i], size_record);

                /// Pointer -> current record_t object
                // const uint8_t *raw = (const uint8_t *)&hi_temp_buffer[i];
                DebugHex16("[FLASH] Writing at adress ", (uint16_t)address);

                DebugHex16("  address = ", (uint16_t)address);
                for (uint8_t j = 0; j < sizeof(record_t); j++)
                {
                    DebugHex("    byte ", tmp[j]);
                }

                bool ok = Flash_PageProgram(address, tmp, size_record);
                if (!ok)
                {
                    DebugUVal("[MODE_HI_TEMP] Flash write failed at index ", i, ".");
                    break;
                }
            }
            Flash_Close();

            /// For Debug: Dump data
            Flash_Open();
            for (uint16_t i = 0; i < hi_temp_buffer_index; i++)
            {
                record_t rec;
                uint32_t address = flash_get_record_address(i);

                uint8_t tmp[sizeof(record_t)];
                Flash_ReadData(address, tmp, sizeof(record_t));
                memcpy(&rec, tmp, sizeof(record_t));
                DebugHex("Float Raw[0]: ", tmp[4]);
                DebugHex("Float Raw[1]: ", tmp[5]);
                DebugHex("Float Raw[2]: ", tmp[6]);
                DebugHex("Float Raw[3]: ", tmp[7]);

                DebugFVal("Rec.Temp = ", rec.temperature, " °C");

                // Debug-Ausgabe
                DebugUVal("[FLASH DUMP] Index     = ", i, "");
                DebugHex16("[FLASH DUMP] Adress    = ", (uint16_t)address);
                DebugUVal("[FLASH DUMP] Timestamp = ", rec.timestamp, " (x5min)");
                DebugFVal("[FLASH DUMP] Temp      = ", rec.temperature, "degC");
                DebugUVal("[FLASH DUMP] Flags     = ", rec.flags, "");
                DebugLn("--------------------------------------------------");
            }
            Flash_Close();
            ////////////////////////////// Debug Dump end

            DebugLn("[MODE_HI_TEMP] RAM-Data copied to external flash");

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
