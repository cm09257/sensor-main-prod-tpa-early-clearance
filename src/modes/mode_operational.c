#include "stm8s_gpio.h"
#include "stm8s_exti.h"
#include "app/state_machine.h"
#include "common/types.h"
#include "modes/mode_operational.h"
// #include "modules/radio.h"
#include "modules/settings.h"
// #include "modules/storage.h"
#include "modules/rtc.h"
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/flash.h"
#include "periphery/hardware_resources.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include <string.h>

// Optional: Definition der Flags für record_t.flags
#define FLAG_NONE 0x00
#define FLAG_SENSOR_ERR 0x02

#define DEV_MODE_OPERATIONAL_TESTING 1

volatile bool mode_operational_rtc_alert_triggered = FALSE;

void mode_operational_run(void)
{
    DebugLn("=============== MODE_OPERATIONAL START ===============");
#if DEV_MODE_OPERATIONAL_TESTING
    DebugLn("[DEV] Override: ALM0 in 1min / ALM1 in 2min");
#endif
    ///////////// Configuring RTC_WAKE pin for EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    ///////////// Measure temperature
    TMP126_OpenForMeasurement();
    float temp_c = TMP126_ReadTemperatureCelsius();
    TMP126_CloseForMeasurement();

    if (temp_c < -100.0f || temp_c > 200.0f)
    {
        DebugLn("[MODE_OPERATIONAL] Sensor fail or invalid measurement.");
        state_transition(MODE_SLEEP);
        return;
    }

    DebugFVal("[MODE_OPERATIONAL] Measured temperature: ", temp_c, "degC");

    ///////////// Get timestamp from RTC
    timestamp_t ts = rtc_get_timestamp();
    DebugUVal("[OPERATIONAL] Timestamp = ", (uint16_t)ts, "*5min");

    ///////////// Create data record
    record_t rec;
    rec.timestamp = ts;
    rec.temperature = temp_c;
    rec.flags = FLAG_NONE;

    ///////////// Prepare data write to external flash
    settings_t *settings = settings_get();
    uint32_t address = flash_get_record_address(settings->flash_record_count);
    uint8_t tmp[sizeof(record_t)];
    memcpy(tmp, &rec, sizeof(record_t));

    ///////////// Write into external flash
    Flash_Open();
    bool ok = Flash_PageProgram(address, tmp, sizeof(record_t));
    Flash_Close();
    if (!ok)
    {
        DebugLn("[OPERATIONAL] Failed writing into external flash.");
    }
    else
    {
        DebugLn("[OPERATIONAL] Record successfully written to external flash.");
    }

    ///////////// Increment flash counter in settings (EEPROM)
    settings->flash_record_count++;
    settings_save();
    settings_load(); // optional, wenn Konsistenz direkt benötigt
    DebugUVal("[OPERATIONAL] Updated flash_record_count = ", settings->flash_record_count, ".");

    ///////////// Determine next measurement time and set alarm
    uint8_t hour, min, sec;
    MCP7940N_Open();
    MCP7940N_ClearAlarmFlagX(RTC_ALARM_0);
    delay(50);
    MCP7940N_DisableAlarmX(RTC_ALARM_0);
    MCP7940N_GetTime(&hour, &min, &sec);
    char buf[32];
    rtc_format_time(buf, hour, min, sec);
    Debug("Current time : ");
    DebugLn(buf);
    uint8_t offset;
#if DEV_MODE_OPERATIONAL_TESTING
    offset = 1;
#else
    offset = settings->measurement_interval_5min * 5;
#endif
    DebugUVal("[MODE_OPERATIONAL] Set next measurement in ", offset, "min");
    min += offset;
    while (min >= 60)
    {
        min -= 60;
        hour = (hour + 1) % 24;
    }
    MCP7940N_ConfigureAbsoluteAlarmX(RTC_ALARM_0, hour, min, 0);
    MCP7940N_Close();
    Debug("Temp. meas. alarm : ");
    rtc_format_time(buf, hour, min, 0);
    DebugLn(buf);

    // === Datentransfer-Alarm (ALM1) setzen ===
    MCP7940N_Open();

    uint8_t tx_hour = 0;
    uint8_t tx_min = 0;

    if (settings->send_mode == 0)
    {
        // periodisch -> aktuellen Zeitpunkt + Offset
        MCP7940N_GetTime(&tx_hour, &tx_min, &sec);
#if DEV_MODE_OPERATIONAL_TESTING
        uint8_t tx_offset = 2;
#else
        uint8_t tx_offset = settings->send_interval_5min * 5;
#endif
        tx_min += tx_offset;
        while (tx_min >= 60)
        {
            tx_min -= 60;
            tx_hour = (tx_hour + 1) % 24;
        }
        DebugLn("[MODE_OPERATIONAL] Set ALM1: periodic mode");
    }
    else
    {
        // feste Uhrzeit
        tx_hour = settings->send_fixed_hour;
        tx_min = settings->send_fixed_minute;
        DebugLn("[MODE_OPERATIONAL] Set ALM1: fixed time mode");
    }
    Debug("Radio time : ");
    rtc_format_time(buf, tx_hour, tx_min, 0);
    DebugLn(buf);
    MCP7940N_ConfigureAbsoluteAlarmX(RTC_ALARM_1, tx_hour, tx_min, 0);

    MCP7940N_Close();

    ///////////// Go to power_halt mode, wakeup using RTC EXTI
    DebugLn("[MODE_HI_TEMP] HALT until next RTC-Alarm");
    power_enter_halt();
    delay(100);
    enableInterrupts();
    mode_operational_rtc_alert_triggered = FALSE;
    __asm__("halt");

    if (!mode_operational_rtc_alert_triggered)
    {
        DebugLn("[MODE_OPERATIONAL] Unexpected wakeup without flag. Sleeping again.");
        return;
    }

    /////////////////////////////////////////// ISR handler

    // RTC: Alarm-Quelle bestimmen
    MCP7940N_Open();
    uint8_t src = 255; // default: kein Alarm erkannt
    if (MCP7940N_IsAlarm0Triggered())
        src = 0;
    else if (MCP7940N_IsAlarm1Triggered())
        src = 1;
    delay(5);
    MCP7940N_DisableAlarmX(src); // immediately disable and clear alarm
    delay(5);
    MCP7940N_ClearAlarmFlagX(src); // in ISR
    delay(5);
    MCP7940N_Close();
    delay(5);

    if (src == 0) // Alarm 0
    {
        DebugLn("[MODE_OPERATIONAL] [ISR] ALM0 -> Temperature measurement");
        state_transition(MODE_OPERATIONAL); // oder wakeup-Flag setzen
    }
    else if (src == 1) // Alarm 1
    {
        DebugLn("[MODE_OPERATIONAL] [ISR] ALM1 -> Data transfer");
        state_transition(MODE_DATA_TRANSFER);
    }
    else
    {
        DebugLn("[MODE_OPERATIONAL] [ISR] Unknown RTC alert.");
    }
}
