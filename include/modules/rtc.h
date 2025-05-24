// rtc.h
#ifndef RTC_H
#define RTC_H

#include "stm8s.h"
#include "common/types.h"  ///< Definition von timestamp_t

/**
 * @file rtc.h
 * @brief Verwaltung von Uhrzeit und zwei unabhängigen Alarmen mit dem MCP7940N
 */

typedef uint8_t rtc_alarm_t;
#define RTC_ALARM_0 0
#define RTC_ALARM_1 1

// ───────────── RTC-Zeitfunktionen ─────────────
void rtc_init(void);
void rtc_get_time(uint8_t* hour, uint8_t* minute, uint8_t* second);
timestamp_t rtc_get_timestamp(void);
void rtc_set_unix_timestamp(uint32_t unix_time);

// ───────────── Alarmfunktionen ─────────────
void rtc_set_alarm(rtc_alarm_t alarm, uint8_t hour, uint8_t minute, uint8_t second);
void rtc_set_alarm_in_minutes(rtc_alarm_t alarm, uint8_t delta_min);
void rtc_set_alarm_offset(rtc_alarm_t alarm, uint8_t offset_minutes, uint8_t offset_seconds);
void rtc_clear_alarm(rtc_alarm_t alarm);
uint8_t rtc_was_alarm_triggered(rtc_alarm_t alarm);

// ────── Periodisch: Nur ALARM 0 ──────
void rtc_set_periodic_alarm_5min(void);
void rtc_set_periodic_alarm_10min(void);
void rtc_set_periodic_alarm_15min(void);
void rtc_set_periodic_alarm_30min(void);
void rtc_set_periodic_alarm_hourly(void);

#endif // RTC_H