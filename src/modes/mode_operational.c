#include "stm8s_gpio.h"
#include "stm8s_exti.h"
#include "app/state_machine.h"
#include "types.h"
#include "modes/mode_operational.h"
#include "modules/settings.h"
#include "modules/rtc.h"
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/flash.h"
#include "periphery/power.h"
#include "periphery/hardware_resources.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include <string.h>

// Optional: Definition der Flags für record_t.flags
#define FLAG_NONE 0x00
#define FLAG_SENSOR_ERR 0x02

#define RADIO_PRIORITY_TOLERANCE_SEC 50

typedef enum
{
    ALARM_TYPE_PERIODIC = 0,
    ALARM_TYPE_FIXED_TIME = 1
} alarm_type_t;

typedef enum
{
    NEXT_ALARM_TEMP = 0,
    NEXT_ALARM_RADIO = 1
} next_alarm_type_t;

typedef struct
{
    uint8_t fixed_alarm_h;        // fixed alarm only
    uint8_t fixed_alarm_m;        // fixed alarm only
    uint8_t fixed_alarm_s;        // fixed alarm only
    alarm_type_t alarm_type;      /// 0 = periodic, 1 = fixed-time
    uint8_t alarm_interval_5_min; // only periodic alarms, in 5min steps
} alarm_t;

volatile bool mode_operational_rtc_alert_triggered = FALSE;

static void rtc_dump_time_if_debug(const char *tag)
{
#if defined(DEBUG_MODE_OPERATIONAL)
    char buf[32];
    rtc_get_format_time(buf);
    Debug(tag);
    DebugLn(buf);
#else
    (void)tag;
#endif
}

/* einmalige RTC-Zeit holen */
static void rtc_get_hms(uint8_t *h, uint8_t *m, uint8_t *s)
{
    MCP7940N_Open();
    MCP7940N_GetTime(h, m, s);
    MCP7940N_Close();
}

// Hilfsfunktion: nächste absolute Sekunde berechnen
static uint32_t calc_next_alarm_sec(const alarm_t *alarm, uint32_t now_sec)
{
    const uint32_t SECONDS_PER_DAY = 24UL * 3600UL;

    if (alarm->alarm_type == ALARM_TYPE_FIXED_TIME)
    {
        uint32_t next_abs =
            (uint32_t)alarm->fixed_alarm_h * 3600UL +
            (uint32_t)alarm->fixed_alarm_m * 60UL +
            (uint32_t)alarm->fixed_alarm_s;

        if (next_abs <= now_sec)
            next_abs += SECONDS_PER_DAY;   //
        return next_abs % SECONDS_PER_DAY; //
    }
    else
    {
        uint32_t interval_sec = (uint32_t)alarm->alarm_interval_5_min * 5UL * 60UL;

        if (interval_sec == 0) /* ungültig → 24 h */
            return (now_sec + SECONDS_PER_DAY) % SECONDS_PER_DAY;

#if defined(DEBUG_CONFIGURATION)
        uint32_t dbg = interval_sec / 2UL;
        DebugULong("DBG: Shrt time to nxt al to ", dbg, "s");
        interval_sec = dbg;
#endif
        uint32_t next_abs =
            ((now_sec / interval_sec) + 1UL) * interval_sec;

        if (next_abs >= SECONDS_PER_DAY) /* über Mitternacht */
            next_abs -= SECONDS_PER_DAY;

        // DebugULong("rt:", next_abs, "");
        return next_abs;
    }
}

void calculate_next_alarm(
    uint8_t curr_h, uint8_t curr_m, uint8_t curr_s,
    uint8_t *next_h, uint8_t *next_m, uint8_t *next_s,
    next_alarm_type_t *next_alarm_type, alarm_t *radio_alarm, alarm_t *measure_temp_alarm)
{
    // Zeit in Sekunden seit Mitternacht
    uint32_t now_sec = curr_h * 3600UL + curr_m * 60UL + curr_s;
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugUVal("DBG: Now = ", now_sec, "s");
#endif

    uint32_t radio_sec = calc_next_alarm_sec(radio_alarm, now_sec);
    // DebugUVal("Rd ", radio_alarm->alarm_interval_5_min, "");
    uint32_t temp_sec = calc_next_alarm_sec(measure_temp_alarm, now_sec);
    // DebugUVal("Tm ", measure_temp_alarm->alarm_interval_5_min, "");
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugULong("Rd al at ", radio_sec, "s");
    DebugULong("Tm al at ", temp_sec, "s");
#endif

    uint32_t chosen;
    if (radio_sec <= temp_sec)
    {
        // Radio sowieso früher: Radio gewinnt
        chosen = radio_sec;
        *next_alarm_type = NEXT_ALARM_RADIO;
        // DebugLn("Rd mch earl-Rd wins");
    }
#if defined(DEBUG_CONFIGURATION)
    else if ((radio_sec - temp_sec) <= RADIO_PRIORITY_TOLERANCE_SEC / 2)
#else
    else if ((radio_sec - temp_sec) <= RADIO_PRIORITY_TOLERANCE_SEC)
#endif
    {
        // Radio kommt knapp später: trotzdem Radio bevorzugen
        chosen = radio_sec;
        *next_alarm_type = NEXT_ALARM_RADIO;
        // DebugLn("Rd close l8t-Rd wins");
    }
    else
    {
        // Temperatur deutlich früher: Temperatur gewinnt
        chosen = temp_sec;
        *next_alarm_type = NEXT_ALARM_TEMP;
        //  DebugLn("Tm mch earl-Tm wins");
    }

    // Sekunden in h:m:s umrechnen
    *next_h = (chosen / 3600UL) % 24;
    *next_m = (chosen % 3600UL) / 60UL;
    *next_s = chosen % 60UL;
#if defined(DEBUG_MODE_OPERATIONAL)
    char buf[32];
    Debug("Alarm:");
    rtc_format_time(buf, *next_h, *next_m, *next_s);
    DebugLn(buf);
#endif
}

bool time_in_window(uint8_t h, uint8_t m,
                    uint8_t start_h, uint8_t start_m,
                    uint8_t stop_h, uint8_t stop_m)
{
    uint16_t now = h * 60 + m;
    uint16_t start = start_h * 60 + start_m;
    uint16_t stop = stop_h * 60 + stop_m;

    if (start <= stop)
    {
        return now >= start && now <= stop;
    }
    else
    {
        // Zeitfenster geht über Mitternacht
        return now >= start || now <= stop;
    }
}

void mode_operational_run(void)
{
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugLn("=MD_OP=");
#endif
    settings_t *settings = settings_get();
    alarm_t radio_alarm, measure_temp_alarm;
    ///////////// Setting alarm config for radio
    radio_alarm.alarm_type = (settings->send_mode == 0) ? ALARM_TYPE_PERIODIC : ALARM_TYPE_FIXED_TIME;
    radio_alarm.alarm_interval_5_min = settings->send_interval_5min;
    radio_alarm.fixed_alarm_h = settings->send_fixed_hour;
    radio_alarm.fixed_alarm_m = settings->send_fixed_minute;
    radio_alarm.fixed_alarm_s = 0;

    ///////////// Setting alarm config for temperature measurement
    measure_temp_alarm.alarm_type = (settings->meas_mode == 0) ? ALARM_TYPE_PERIODIC : ALARM_TYPE_FIXED_TIME;
    measure_temp_alarm.alarm_interval_5_min = settings->meas_interval_5min;
    measure_temp_alarm.fixed_alarm_h = settings->meas_fixed_hour;
    measure_temp_alarm.fixed_alarm_m = settings->meas_fixed_minute;
    measure_temp_alarm.fixed_alarm_s = 0;

    ///////////// Debug RTC alarm status
    //  MCP7940N_Open();
    //  DebugLn(MCP7940N_IsAlarm0Triggered() ? "[DEBUG] ALM0TRIG = YES" : "[DEBUG] ALM0TRIG = NO");
    // DebugLn(MCP7940N_IsAlarmEnabled(0) ? "[DEBUG] ALM0EN = YES" : "[DEBUG] ALM0EN = NO");
    // MCP7940N_Close();

    ///////////// Configuring RTC_WAKE pin for EXTI
    GPIO_Init(RTC_WAKE_PORT, RTC_WAKE_PIN, GPIO_MODE_IN_FL_IT);
    EXTI_SetExtIntSensitivity(RTC_EXTI_PORT, EXTI_SENSITIVITY_FALL_ONLY);

    ///////////// Measure temperature
    TMP126_OpenForMeasurement();
    float temp_c = TMP126_ReadTemperatureCelsius();
    TMP126_CloseForMeasurement();

    if (temp_c < -100.0f || temp_c > 200.0f)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]tmp meas err");
#endif
        return;
    }
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugFVal("[MDOP]Meas tmp:", temp_c, "");
#endif

    ///////////// Get timestamp from RTC
    timestamp_t ts = rtc_get_timestamp();
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugUVal("[MDOP]Timest=", (uint16_t)ts, "*5mn");
#endif
    ///////////// Create data record
    record_t rec;
    rec.timestamp = ts;
    rec.temperature = temp_c;
    rec.flags = FLAG_NONE;

    ///////////// Prepare data write to external flash
    settings = settings_get();
    uint32_t address = flash_get_record_address(settings->flash_record_count);
    uint8_t tmp[sizeof(record_t)];
    memcpy(tmp, &rec, sizeof(record_t));

    ///////////// Write into external flash
    Flash_Open();
    bool ok = Flash_PageProgram(address, tmp, sizeof(record_t));
    Flash_Close();
#if defined(DEBUG_MODE_OPERATIONAL)
    if (!ok)
    {
        DebugLn("[MDOP]Rec>flsh err");
    }
    else
    {
        DebugLn("[MDOP]Rec>flsh ok");
    }
#endif

    ///////////// Increment flash counter in settings (EEPROM)
    settings->flash_record_count++;
    settings_save();
    settings_load(); // optional, wenn Konsistenz direkt benötigt
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugUVal("[MDOP]Upd.rec_cnt=", settings->flash_record_count, ".");
#endif
    ///////////// Determine next alarm type and time
    uint8_t curr_h, curr_m, curr_s;
    MCP7940N_Open();
    MCP7940N_GetTime(&curr_h, &curr_m, &curr_s);
    MCP7940N_DisableAlarmX(RTC_ALARM_0);
    MCP7940N_ClearAlarmFlagX(RTC_ALARM_0);

    uint8_t next_h, next_m, next_s;
    next_alarm_type_t alarm_typ;
    // DebugUVal("RdTy:", radio_alarm.alarm_type, "");
    // DebugUVal("TmTy:", measure_temp_alarm.alarm_type, "");
    calculate_next_alarm(curr_h, curr_m, curr_s, &next_h, &next_m, &next_s, &alarm_typ, &radio_alarm, &measure_temp_alarm);
    disableInterrupts();
    MCP7940N_ConfigureAbsoluteAlarmX(RTC_ALARM_0, next_h, next_m, next_s);
    MCP7940N_Close();

    char buf[64];
#if defined(DEBUG_MODE_OPERATIONAL)
    if (alarm_typ == NEXT_ALARM_TEMP)
        DebugLn("[MDOP]Nxt alarm: Temp meas.");
    else
        DebugLn("[MDOP]Nxt alarm: Data xfer.");
#endif
    //  MCP7940N_Open();
    // #if defined(DEBUG_MODE_OPERATIONAL)
    //   DebugLn(MCP7940N_IsAlarm0Triggered() ? "[DEBUG] ALM0TRIG = YES" : "[DEBUG] ALM0TRIG = NO");
    // DebugLn(MCP7940N_IsAlarmEnabled(RTC_ALARM_0) ? "[DEBUG] ALM0EN = YES" : "[DEBUG] ALM0EN = NO");
    // #endif
    //   MCP7940N_Close();

#if defined(DEBUG_MODE_OPERATIONAL)
    rtc_get_format_time(buf);
    Debug("Time :");
    DebugLn(buf);
#endif

///////////// Go to power_halt mode, wakeup using RTC EXTI
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugLn("[MDOP]HALT");
#endif
    power_enter_halt();
    delay(100);
    enableInterrupts();
    mode_before_halt = MODE_OPERATIONAL;
    mode_operational_rtc_alert_triggered = FALSE;
    __asm__("halt");
    disableInterrupts();
    MCP7940N_Open();
    MCP7940N_DisableAlarmX(0);
    MCP7940N_ClearAlarmFlagX(0);
    MCP7940N_Close();

#if defined(DEBUG_MODE_OPERATIONAL)
    Debug("woke ");
    MCP7940N_Open();
    rtc_get_format_time(buf);
    MCP7940N_Close();
    DebugLn(buf);
#endif

    ///////////// Evaluate alarm type and state transition
    if (alarm_typ == NEXT_ALARM_TEMP)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]Int->Tmp meas");
#endif
        state_transition(MODE_OPERATIONAL);
    }
    else if (alarm_typ == NEXT_ALARM_RADIO)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]Int->Dt xfr");
#endif

        if (settings->timewindow_active)
        {
            // Hole aktuelle Uhrzeit
            uint8_t h, m, s;
            MCP7940N_Open();
            MCP7940N_GetTime(&h, &m, &s);
            MCP7940N_Close();

            bool in_window = time_in_window(
                h, m,
                settings->send_time_window_from_hour, 0,
                settings->send_time_window_until_hour, 0);

            if (!in_window)
            {
#if defined(DEBUG_MODE_OPERATIONAL)
                DebugLn("[MDOP]Out of tx wndw.stay in op");
#endif
                state_transition(MODE_OPERATIONAL);
                return;
            }
        }

        state_transition(MODE_DATA_TRANSFER);
        return;
    }
    else
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]Unknwn al type");
#endif
        nop();
    }
}
