// mode_high_temperature.c

#include "stm8s_exti.h"
#include "stm8s_gpio.h"
#include "modes/mode_high_temperature.h"
// #include "modules/storage_internal.h"
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

#define DEV_HI_TEMP_SKIP_AFTER 3
#define HI_TEMP_RAM_BUFFER_SIZE 72

volatile bool mode_hi_temp_measurement_alert_triggered = FALSE;
static uint8_t dev_hi_temp_counter = 0;
static record_t hi_temp_buffer[HI_TEMP_RAM_BUFFER_SIZE];
static uint8_t hi_temp_buffer_index = 0;

void mode_high_temperature_run(void)
{
#if defined(DEBUG_MODE_HI_TEMP)
    DebugLn("=HI_TMP=");
#endif

    ///////////// Resetting index RAM Buffer for data records in Hi-Temp mode (no access to flash in hi-temp mode)
    hi_temp_buffer_index = 0;
    ///////////// Clearing buffer
    memset(hi_temp_buffer, 0, sizeof(hi_temp_buffer));

    ///////////// Loading settings
    settings_t *settings = settings_get();
    float threshold = settings->cool_down_threshold;
    uint8_t interval_min = settings->high_temp_measurement_interval_5min * 5;
#if defined(DEBUG_MODE_HI_TEMP)
    DebugFVal("[HITMP]LoTmpThre=", threshold, "");
#endif

    ///////////// Configuring RTC_WAKE pin for EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

///////////// Main loop
#if defined(DEBUG_MODE_HI_TEMP)
    DebugLn("[HITMP]DaqCoolDwn");
#endif
    while (state_get_current() == MODE_HIGH_TEMPERATURE)
    {
        ///////////// Measure Temperature
        TMP126_OpenForMeasurement();
        float temp = TMP126_ReadTemperatureCelsius();
        TMP126_CloseForMeasurement();
#if defined(DEBUG_MODE_HI_TEMP)
        DebugFVal("[HITMP]RdTmp=", temp, "");
#endif

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
#if defined(DEBUG_MODE_HI_TEMP)
            DebugLn("[HITMP]RamFull");
#endif
            nop();
        }

        ///////////// Development phase: After three measurements --> Copy data and transition to MODE_OPERATIONAL // TODO: Remove counter

#if defined(DEBUG_CONFIGURATION)
        dev_hi_temp_counter++;
#if defined(DEBUG_MODE_HI_TEMP)
        DebugUVal("[HITMP][DEV]HiTmpMeasCnt:", dev_hi_temp_counter, "");
#endif
        if (dev_hi_temp_counter >= DEV_HI_TEMP_SKIP_AFTER)
        {
#if defined(DEBUG_MODE_HI_TEMP)
            DebugLn("[HITMP][DEV]ThresIgnored>GoTo Copy&Chng Mode");
#endif
            temp = threshold - 1; // Schwellenwert künstlich unterschreiten
        }
#endif

        ///////////// Check if temperature is below threshold
        if (temp < threshold)
        {
///////////// Copy data from RAM --> Ext. Flash
#if defined(DEBUG_MODE_HI_TEMP)
            DebugLn("[HITMP]Tmp<thres->Copy dt & chng mode");
#endif

            /// Adress in flash, calculation in for loop.
            uint32_t address;
            uint8_t size_record = sizeof(record_t);

            Flash_Open();
            for (uint16_t i = 0; i < hi_temp_buffer_index; i++)
            {
                /// Compute adress in flash, taking page limits into account.
                address = flash_get_record_address(settings->flash_record_count + i);

                /// Serialize record
                uint8_t tmp[sizeof(record_t)];
                memcpy(tmp, &hi_temp_buffer[i], size_record);

                bool ok = Flash_PageProgram(address, tmp, size_record);
                if (!ok)
                {
#if defined(DEBUG_MODE_HI_TEMP)
                    DebugUVal("[HITMP]FlshWrtErr at i=", i, "");
#endif
                    break;
                }
            }
            Flash_Close();

            /// For Debug: Dump data
            Flash_Open();
            for (uint16_t i = 0; i < hi_temp_buffer_index; i++)
            {
                record_t rec;
                uint32_t address = flash_get_record_address(settings->flash_record_count + i);

                uint8_t tmp[sizeof(record_t)];
                Flash_ReadData(address, tmp, sizeof(record_t));
                memcpy(&rec, tmp, sizeof(record_t));

// Debug-Ausgabe
#if defined(DEBUG_MODE_HI_TEMP)
                // DebugUVal("[FLASH DUMP] Index     = ", i, "");
                //  DebugHex16("[FLASH DUMP] Adress    = ", (uint16_t)address);
                // DebugUVal("[FLASH DUMP] Timestamp = ", rec.timestamp, " (x5min)");
                //  DebugFVal("[FLASH DUMP] Temp      = ", rec.temperature, "degC");
                //  DebugUVal("[FLASH DUMP] Flags     = ", rec.flags, "");
                //  DebugLn("--------------------------------------------------");
#endif
            }
            Flash_Close();
            ////////////////////////////// Debug Dump end
#if defined(DEBUG_MODE_HI_TEMP)
            DebugLn("[HITMP]RAM>ext.fl");
#endif
            settings->flash_record_count += hi_temp_buffer_index;
            settings_save();
            settings_load();
            settings = settings_get();
#if defined(DEBUG_MODE_HI_TEMP)
            DebugUVal("[HITMP]UpdFlRecCnt=", settings->flash_record_count, "");
#endif
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
#if defined(DEBUG_MODE_HI_TEMP)
        DebugUVal("[HI_TMP]Next Wake in ", interval_min, "mn");
#endif
        rtc_set_alarm_in_minutes(RTC_ALARM_1, interval_min);
#endif
        mode_before_halt = MODE_HIGH_TEMPERATURE;

///// Go to power_halt mode, wakeup using RTC EXTI
#if defined(DEBUG_MODE_HI_TEMP)
        DebugLn("[HI_TMP]HALT");
#endif
        power_enter_halt();
        delay(100);
        enableInterrupts();
        __asm__("halt");

        ///// After HALT: RTC-Interrupt was triggered
        MCP7940N_Open();
        delay(5);
        MCP7940N_DisableAlarmX(RTC_ALARM_1); // immediately disable and clear alarm
        delay(5);
        MCP7940N_ClearAlarmFlagX(RTC_ALARM_1); // in ISR
        delay(5);
        MCP7940N_Close();
        delay(5);

        //  DebugLn("[MODE_HI_TEMP] Restarting main loop");
        mode_hi_temp_measurement_alert_triggered = FALSE;
    }
}
