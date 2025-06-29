#include "stm8s_gpio.h"
#include "stm8s_exti.h"
#include "app/state_machine.h"
#include "types.h"
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

static next_alarm_type_t last_set_alarm_type;

typedef struct
{
    uint8_t fixed_alarm_h;        // fixed alarm only
    uint8_t fixed_alarm_m;        // fixed alarm only
    uint8_t fixed_alarm_s;        // fixed alarm only
    alarm_type_t alarm_type;      /// 0 = periodic, 1 = fixed-time
    uint8_t alarm_interval_5_min; // only periodic alarms, in 5min steps
} alarm_t;

static alarm_t radio_alarm;

static alarm_t measure_temp_alarm;

volatile bool mode_operational_rtc_alert_triggered = FALSE;

// Hilfsfunktion: nächste absolute Sekunde berechnen
static uint32_t calc_next_alarm_sec(const alarm_t *alarm, uint32_t now_sec)
{
    if (alarm->alarm_type == ALARM_TYPE_FIXED_TIME)
    {
        uint32_t next = alarm->fixed_alarm_h * 3600UL + alarm->fixed_alarm_m * 60UL + alarm->fixed_alarm_s;
        if (next <= now_sec)
            next += 24UL * 3600UL; // auf nächsten Tag verschieben
        return next;
    }
    else
    {
        uint16_t interval_sec = alarm->alarm_interval_5_min * 5 * 60;

#if defined(DEBUG_CONFIGURATION)
        //DebugUVal("[DEBUG] Shortened seconds to next alarm from ", interval_sec, "sec");
        interval_sec = interval_sec / 10;
      //  DebugUVal("[DEBUG] to ", interval_sec, "sec.");
#endif

        uint32_t next = ((now_sec / interval_sec) + 1) * interval_sec;
        return next;
    }
}

void calculate_next_alarm(
    uint8_t curr_h, uint8_t curr_m, uint8_t curr_s,
    uint8_t *next_h, uint8_t *next_m, uint8_t *next_s,
    next_alarm_type_t *next_alarm_type)
{
    // Zeit in Sekunden seit Mitternacht
    uint32_t now_sec = curr_h * 3600UL + curr_m * 60UL + curr_s;
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugUVal("[Debug] Now sec = ", now_sec, "sec");
#endif

    uint32_t radio_sec = calc_next_alarm_sec(&radio_alarm, now_sec);
    uint32_t temp_sec = calc_next_alarm_sec(&measure_temp_alarm, now_sec);
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugUVal("[DEBUG] Radio alarm in ", (uint16_t)radio_sec, "sec");
    DebugUVal("[DEBUG] Temp alarm in ", (uint16_t)temp_sec, "sec");
#endif

    uint32_t chosen;
    if (radio_sec <= temp_sec)
    {
        // Radio sowieso früher: Radio gewinnt
        chosen = radio_sec;
        *next_alarm_type = NEXT_ALARM_RADIO;
    }
    else if ((radio_sec - temp_sec) <= RADIO_PRIORITY_TOLERANCE_SEC)
    {
        // Radio kommt knapp später: trotzdem Radio bevorzugen
        chosen = radio_sec;
        *next_alarm_type = NEXT_ALARM_RADIO;
    }
    else
    {
        // Temperatur deutlich früher: Temperatur gewinnt
        chosen = temp_sec;
        *next_alarm_type = NEXT_ALARM_TEMP;
    }

    // Sekunden in h:m:s umrechnen
    *next_h = (chosen / 3600UL) % 24;
    *next_m = (chosen % 3600UL) / 60UL;
    *next_s = chosen % 60UL;
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugUVal("Next_h = ", *next_h, "h");
    DebugUVal("Next_m = ", *next_m, "m");
    DebugUVal("Next_s = ", *next_s, "s");
#endif
}

void mode_operational_run(void)
{
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugLn("=MD_OP=");
#endif
    settings_t *settings = settings_get();

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
    MCP7940N_ClearAlarmFlagX(RTC_ALARM_0);
    MCP7940N_DisableAlarmX(RTC_ALARM_0);

    uint8_t next_h, next_m, next_s;
    next_alarm_type_t alarm_typ;
    calculate_next_alarm(curr_h, curr_m, curr_s, &next_h, &next_m, &next_s, &alarm_typ);
    disableInterrupts();
    MCP7940N_ConfigureAbsoluteAlarmX(RTC_ALARM_0, next_h, next_m, next_s);
    MCP7940N_Close();

    char buf[64];
    rtc_format_time(buf, next_h, next_m, next_s);
#if defined(DEBUG_MODE_OPERATIONAL)
   // Debug("[OPERATIONAL] Next alarm set : ");
    DebugLn(buf);
    if (alarm_typ == NEXT_ALARM_TEMP)
        DebugLn("[MDOP]Nxt al MEAS_TMP");
    else
        DebugLn("[MDOP]Nxt al DT_XFR");
#endif
    MCP7940N_Open();
#if defined(DEBUG_MODE_OPERATIONAL)
    DebugLn(MCP7940N_IsAlarm0Triggered() ? "[DEBUG] ALM0TRIG = YES" : "[DEBUG] ALM0TRIG = NO");
    DebugLn(MCP7940N_IsAlarmEnabled(RTC_ALARM_0) ? "[DEBUG] ALM0EN = YES" : "[DEBUG] ALM0EN = NO");
#endif
    MCP7940N_Close();

#if defined(DEBUG_MODE_OPERATIONAL)
    rtc_get_format_time(buf);
    Debug("[MDOP]");
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
    last_set_alarm_type = alarm_typ;
    __asm__("halt");

#if defined(DEBUG_MODE_OPERATIONAL)
    Debug("[MDOP]");
    MCP7940N_Open();
    rtc_get_format_time(buf);
    MCP7940N_Close();
    DebugLn(buf);
#endif

    if (!mode_operational_rtc_alert_triggered)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]RTC fail");
#endif
        return;
    }

    ///////////// Obtain alarm trigger, reset and disable alarm
    MCP7940N_Open();
    bool triggered = MCP7940N_IsAlarm0Triggered();
    MCP7940N_DisableAlarmX(RTC_ALARM_0);
    MCP7940N_ClearAlarmFlagX(RTC_ALARM_0);
    MCP7940N_Close();

    if (!triggered)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]RTC fail");
#endif
        return;
    }

    ///////////// Evaluate alarm type and state transition
    if (last_set_alarm_type == NEXT_ALARM_TEMP)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]Int->Tmp meas");
#endif
        state_transition(MODE_OPERATIONAL);
    }
    else if (last_set_alarm_type == NEXT_ALARM_RADIO)
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]Int->Dt xfr");
#endif
        state_transition(MODE_DATA_TRANSFER);
    }
    else
    {
#if defined(DEBUG_MODE_OPERATIONAL)
        DebugLn("[MDOP]Unknwn al type");
#endif
        nop();
    }
}
