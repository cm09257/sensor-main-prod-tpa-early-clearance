#include "modules/rtc.h"
#include "modules/helper_functions.h"
#include "periphery/mcp7940n.h"
#include "stm8s_exti.h"
#include "stm8s_gpio.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "periphery/hardware_resources.h"

void rtc_format_time(char *buf, uint8_t h, uint8_t m, uint8_t s)
{
    buf[0] = '[';
    buf[1] = '0' + h / 10;
    buf[2] = '0' + h % 10;
    buf[3] = ':';
    buf[4] = '0' + m / 10;
    buf[5] = '0' + m % 10;
    buf[6] = ':';
    buf[7] = '0' + s / 10;
    buf[8] = '0' + s % 10;
    buf[9] = ']';
    buf[10] = '\0';
}

void rtc_get_format_time(char *buf)
{
    uint8_t h, m, s;
    MCP7940N_Open();
    MCP7940N_GetTime(&h, &m, &s);
    MCP7940N_Close();
    rtc_format_time(buf, h, m, s);
}

timestamp_t rtc_get_timestamp(void)
{
    //////// Reference time/date: 01.01.2025 00:00:00
    uint8_t h, m, s;
    uint8_t day, month, yr_since_2000;
    uint8_t wkday;
    uint16_t yr_calendar;

    //////// Get date, time
    MCP7940N_Open();
    delay(1);
    MCP7940N_GetDate(&wkday, &day, &month, &yr_since_2000);
    delay(1);
    yr_calendar = yr_since_2000 + 2000;
    MCP7940N_GetTime(&h, &m, &s);
    delay(1);
    MCP7940N_Close();
    delay(1);

    //////// Sanity Check
    if (yr_calendar < 2025)
        return 0;

    //////// Compute Number of Minutes since 01.01.2025 00:00:00
    uint32_t minutes_total = 0;
    uint16_t year = 2025;

    while (year < yr_calendar)
    {
        minutes_total += (is_leap_year(year) ? 366 : 365) * 1440UL;
        year++;
    }

    // Add days of current year
    uint16_t days_this_year = 0;
    static const uint8_t days_per_month[12] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31};

    for (uint8_t i = 0; i < (month - 1); ++i)
        days_this_year += days_per_month[i];

    // Add leap day if past February in leap year
    if (month > 2 && is_leap_year(yr_calendar))
        days_this_year++;

    days_this_year += (day - 1);

    minutes_total += days_this_year * 1440UL;
    minutes_total += h * 60UL + m;

    timestamp_t ts_5min = minutes_total / 5;

    return ts_5min;
}

void rtc_set_alarm_in_minutes(rtc_alarm_t alarm, uint16_t delta_min)
{
    uint8_t h, m, s;
    MCP7940N_Open();
    MCP7940N_GetTime(&h, &m, &s);

    /* add 2 s Puffer und ggf. Übertrag */
    s += 2;                       // 59 + 2 = 61
    if (s >= 60) {
        s -= 60;                  // 61 -> 1
        ++delta_min;              // Übertrag: +1 Minute
    }

    uint32_t total = (uint32_t)m + delta_min;
    uint8_t new_m = total % 60;
    uint8_t new_h = (h + total / 60) % 24;
    MCP7940N_ClearAlarmFlagX(0);
    MCP7940N_ClearAlarmFlagX(1);
    MCP7940N_DisableAlarmX(0);
    MCP7940N_DisableAlarmX(1);
    MCP7940N_ConfigureAbsoluteAlarmX(alarm, new_h, new_m, s);
    char buf[32];
    Debug("Alarm:");
    rtc_format_time(buf, new_h, new_m, s);
    DebugLn(buf);
    MCP7940N_Close();
}


/*
void rtc_set_alarm_offset(rtc_alarm_t alarm, uint8_t offset_minutes, uint8_t offset_seconds)
{
    Debug("[RTC] Alarm-Offset: ");
    char msg[32];
    rtc_format_time(msg, 0, offset_minutes, offset_seconds);
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
}*/

/*void rtc_clear_alarm(rtc_alarm_t alarm)
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
}*/

/*uint8_t rtc_was_alarm_triggered(rtc_alarm_t alarm)
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
}*/

// ────── Nur ALARM 0 für wiederkehrende Zeitfenster ──────
/*static void rtc_set_alarm_periodic(uint8_t interval_min)
{
    uint8_t h, m, s;
    rtc_get_time(&h, &m, &s);

    uint8_t next_min = ((m / interval_min) + 1) * interval_min;
    h = (h + (next_min / 60)) % 24;
    next_min %= 60;

    DebugUVal("[RTC] Setze Periodic-Alarm (min)", interval_min, "");
    rtc_set_alarm(RTC_ALARM_0, h, next_min, 0);
}*/

/*
void rtc_set_periodic_alarm_5min(void) { rtc_set_alarm_periodic(5); }
void rtc_set_periodic_alarm_10min(void) { rtc_set_alarm_periodic(10); }
void rtc_set_periodic_alarm_15min(void) { rtc_set_alarm_periodic(15); }
void rtc_set_periodic_alarm_30min(void) { rtc_set_alarm_periodic(30); }
void rtc_set_periodic_alarm_hourly(void) { rtc_set_alarm_periodic(60); }
*/
/*
void rtc_set_unix_timestamp(uint32_t unix_time)
{
    // Umwandlung von Unix-Zeit (Sekunden seit 1970) in Stunde/Minute/Sekunde
    uint32_t total_seconds = unix_time % 86400; // Sekunden seit Mitternacht
    uint8_t hours = (total_seconds / 3600) % 24;
    uint8_t minutes = (total_seconds / 60) % 60;
    uint8_t seconds = total_seconds % 60;

    char buf[64];
    Debug("[RTC.c] New time set via UNIX timestamp = ");
    rtc_format_time(buf, hours, minutes, seconds);
    DebugLn(buf);

    delay(5);
    MCP7940N_Open();
    delay(5);
    MCP7940N_SetTime(hours, minutes, seconds);
    delay(5);
    MCP7940N_Close();
    delay(5);

    DebugIVal("[RTC] Zeit gesetzt auf (Unix)", (int)unix_time, "s");
}*/
/*
static void rtc_enable_exti(void)
{
    DebugLn("[RTC] EXTI Init (GPIOA, Pin 1)");
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOA, EXTI_SENSITIVITY_FALL_LOW);
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
}*/

/*
void rtc_init(void)
{
    DebugLn("[RTC] Initialisiere MCP7940N...");
    MCP7940N_Init();
    rtc_enable_exti();
}*/

/*
void rtc_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    //    DebugLn("In rtc_get_time");
    MCP7940N_Open();
    //   DebugLn("rtc_get_time open done");
    delay(5);
    MCP7940N_GetTime(hour, minute, second);
    //  DebugLn("rtc_get_time MCP7940N_GetTime done");
    delay(5);
    MCP7940N_Close();
    delay(5);
    // DebugLn("rtc_get_time open done");

    //  DebugLn("Leaving rtc_get_time");
}*/

/*
void rtc_clear_and_disable_alarm(uint8_t alarm)
{
    MCP7940N_Open();
    delay(5);
    MCP7940N_DisableAlarmX(alarm); // immediately disable and clear alarm
    delay(5);
    MCP7940N_ClearAlarmFlagX(alarm); // in ISR
    delay(5);
    MCP7940N_Close();
    delay(5);
}*/

/*
void rtc_set_alarm(rtc_alarm_t alarm, uint8_t hour, uint8_t minute, uint8_t second)
{
    // DebugUVal("[RTC] Setze Alarm", alarm, "");
    // char buf[20];
    // (buf, "-> %02u:%02u:%02u", hour, minute, second);
    // DebugLn(buf);
    MCP7940N_Open();
    delay(5);
    MCP7940N_ConfigureAbsoluteAlarmX(alarm, hour, minute, second);
    //   DebugLn("Configured abs alarm");
    delay(5);
    MCP7940N_Close();
    delay(5);
}*/