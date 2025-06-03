#include "modules/rtc.h"
#include "periphery/mcp7940n.h"
#include "stm8s_exti.h"
#include "stm8s_gpio.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "periphery/hardware_resources.h"

static void rtc_enable_exti(void)
{
    DebugLn("[RTC] EXTI Init (GPIOA, Pin 1)");
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOA, EXTI_SENSITIVITY_FALL_LOW);
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
}

void rtc_init(void)
{
    DebugLn("[RTC] Initialisiere MCP7940N...");
    MCP7940N_Init();
    rtc_enable_exti();
}

void rtc_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
      DebugLn("In rtc_get_time");
    MCP7940N_Open();
    DebugLn("rtc_get_time open done");
    delay(5);
    MCP7940N_GetTime(hour, minute, second);
    DebugLn("rtc_get_time MCP7940N_GetTime done");
    delay(5);
    MCP7940N_Close();
    delay(5);
    DebugLn("rtc_get_time open done");

    //  DebugLn("Leaving rtc_get_time");
    // Debug("[RTC] Aktuelle Uhrzeit: ");
    //   char buf[16];
    // sprintf(buf, "%02u:%02u:%02u", *hour, *minute, *second);
    //  DebugLn(buf);
}

timestamp_t rtc_get_timestamp(void)
{
    uint8_t h, m, s;
    rtc_get_time(&h, &m, &s);
    uint32_t ts = ((uint32_t)h * 60 + m) / 5;

    // Debug("[RTC] Timestamp (5-min): ");
    // char ts_str[12];
    // sprintf(ts_str, "%lu", (unsigned long)ts);
    // DebugLn(ts_str);

    return ts;
}

void rtc_set_alarm(rtc_alarm_t alarm, uint8_t hour, uint8_t minute, uint8_t second)
{
    // DebugUVal("[RTC] Setze Alarm", alarm, "");
    // char buf[20];
    // sprintf(buf, "-> %02u:%02u:%02u", hour, minute, second);
    // DebugLn(buf);
    MCP7940N_Open();
    delay(5);
    MCP7940N_ConfigureAbsoluteAlarmX(alarm, hour, minute, second);
    //   DebugLn("Configured abs alarm");
    delay(5);
    MCP7940N_Close();
    delay(5);
}

void rtc_set_alarm_in_minutes(rtc_alarm_t alarm, uint8_t delta_min)
{
    uint8_t h, m, s;
    rtc_get_time(&h, &m, &s);

    uint8_t new_m = (m + delta_min) % 60;
    uint8_t new_h = (h + (m + delta_min) / 60) % 24;

    DebugUVal("Alarm m = ", new_m, "");
    DebugUVal("Alarm s", s, "");
    rtc_set_alarm(alarm, new_h, new_m, s);
}

void rtc_set_alarm_offset(rtc_alarm_t alarm, uint8_t offset_minutes, uint8_t offset_seconds)
{
    Debug("[RTC] Alarm-Offset: ");
    char msg[32];
    sprintf(msg, "%u min, %u s", offset_minutes, offset_seconds);
    DebugLn(msg);

    uint8_t h, m, s;
    rtc_get_time(&h, &m, &s);

    // aktuelle Zeit und offset in sekunden umrechnen
    uint32_t total_seconds = (uint32_t)h * 3600UL + (uint32_t)m * 60UL + s;
    uint32_t offset_total = (uint32_t)offset_minutes * 60UL + offset_seconds;

    // beide addieren und auf 24h begrenzen
    total_seconds = (total_seconds + offset_total) % 86400UL;

    uint8_t new_h, new_m, new_s;
    // Rückumrechnung in hh:mm:ss
    new_h = (uint8_t)(total_seconds / 3600UL);
    uint32_t remaining = total_seconds % 3600UL;
    new_m = (uint8_t)(remaining / 60UL);
    new_s = (uint8_t)(remaining % 60UL);

    rtc_set_alarm(alarm, new_h, new_m, new_s);
    MCP7940N_Open();
    delay(5);
    MCP7940N_EnableAlarmX(alarm);
    delay(5);
    MCP7940N_Close();
    delay(5);
}

void rtc_clear_alarm(rtc_alarm_t alarm)
{
    DebugUVal("[RTC] Lösche Alarm", alarm, "");
    MCP7940N_Open();
    delay(5);
    MCP7940N_ClearAlarmFlagX(alarm);
    delay(5);
    MCP7940N_DisableAlarmX(alarm);
    delay(5);
    MCP7940N_Close();
    delay(5);
}

uint8_t rtc_was_alarm_triggered(rtc_alarm_t alarm)
{
    MCP7940N_Open();
    delay(5);
    uint8_t result = (alarm == RTC_ALARM_0)
                         ? MCP7940N_IsAlarm0Triggered()
                         : MCP7940N_IsAlarm1Triggered();

    DebugUVal("[RTC] Alarm ausgelöst?", result, "");
    delay(5);
    MCP7940N_Close();
    delay(5);

    return result;
}

// ────── Nur ALARM 0 für wiederkehrende Zeitfenster ──────
static void rtc_set_alarm_periodic(uint8_t interval_min)
{
    uint8_t h, m, s;
    rtc_get_time(&h, &m, &s);

    uint8_t next_min = ((m / interval_min) + 1) * interval_min;
    h = (h + (next_min / 60)) % 24;
    next_min %= 60;

    DebugUVal("[RTC] Setze Periodic-Alarm (min)", interval_min, "");
    rtc_set_alarm(RTC_ALARM_0, h, next_min, 0);
}

void rtc_set_periodic_alarm_5min(void) { rtc_set_alarm_periodic(5); }
void rtc_set_periodic_alarm_10min(void) { rtc_set_alarm_periodic(10); }
void rtc_set_periodic_alarm_15min(void) { rtc_set_alarm_periodic(15); }
void rtc_set_periodic_alarm_30min(void) { rtc_set_alarm_periodic(30); }
void rtc_set_periodic_alarm_hourly(void) { rtc_set_alarm_periodic(60); }

void rtc_set_unix_timestamp(uint32_t unix_time)
{
    // Umwandlung von Unix-Zeit (Sekunden seit 1970) in Stunde/Minute/Sekunde
    uint32_t total_seconds = unix_time % 86400; // Sekunden seit Mitternacht
    uint8_t hours = (total_seconds / 3600) % 24;
    uint8_t minutes = (total_seconds / 60) % 60;
    uint8_t seconds = total_seconds % 60;

    delay(5);
    MCP7940N_Open();
    delay(5);
    MCP7940N_SetTime(hours, minutes, seconds);
    delay(5);
    MCP7940N_Close();
    delay(5);

    DebugIVal("[RTC] Zeit gesetzt auf (Unix)", (int)unix_time, "s");
}
