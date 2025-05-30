// src/modes/mode_pre_high_temperature.c
#include "stm8s_gpio.h"
#include "stm8s_exti.h"
#include "periphery/hardware_resources.h"
#include "app/state_machine.h"
#include "modes/mode_pre_high_temperature.h"
#include "modules/settings.h"
#include "modules/storage_internal.h"
#include "modules/rtc.h"
#if defined(PCB_REV_2_5)
#include "modules/interrupts_PCB_REV_2_5.h"
#elif defined(PCB_REV_3_1)
#include "modules/interrupts_PCB_REV_3_1.h"
#endif
#include "periphery/tmp126.h"
#include "utility/delay.h"

#include "utility/debug.h"

volatile bool pre_hi_temp_alert_triggered = FALSE;

void TMP126_DebugWakePin(void)
{

    GPIO_Init(TMP126_WAKE_PORT, TMP126_WAKE_PIN, GPIO_MODE_IN_PU_NO_IT);

    if (GPIO_ReadInputPin(TMP126_WAKE_PORT, TMP126_WAKE_PIN) == SET)
    {
        DebugLn("[TMP126 WAKE] High");
    }
    else
    {
        DebugLn("[TMP126 WAKE] Low");
    }
}

void mode_pre_high_temperature_run(void)
{
    Debug("=== Mode PRE_HIGH_TEMP] ===");

    TMP126_OpenForAlert(); // Set mode for using alert functions of tmp126

    uint16_t alertStatus = TMP126_ReadAlertStatus(); // Read Alert Status clears all alert flags
    delay(5);
    TMP126_SetHysteresis(1.0f); // Set Hysteresis to 1K
    delay(5);

    TMP126_SetHiLimit(PRE_HIGH_TEMP_THRESHOLD_C);
    delay(5);
    TMP126_SetLoLimit(-60.0f);

    TMP126_Enable_THigh_Alert();
    delay(5);
    TMP126_Disable_TLow_Alert();
    delay(5);
    DebugLn(TMP126_Is_THigh_Enabled() ? "THi Alert Enabled" : "THi Alert Disabled");
    delay(5);
    DebugLn(TMP126_Is_TLow_Enabled() ? "TLo Alert Enabled" : "TLo Alert Disabled");
    delay(5);

    // TODO: Wenn der EXTI Port funktioniert, angepasst wieder einbinden
    // GPIO_Init(TMP126_WAKE_PORT, TMP126_WAKE_PIN, GPIO_MODE_IN_PU_IT); // WAKE_TMP as input with interrupt
    // EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOE, EXTI_SENSITIVITY_FALL_ONLY);
    // enableInterrupts();

    uint8_t h, m, s;
    char buf[64];

    float curr_temp = 0.0f; // TODO: Remove, wenn exti funktioniert
    bool alert_active = FALSE;

    while (1)
    {
        curr_temp = TMP126_ReadTemperatureCelsius(); // TODO: Remove, wenn exti funktioniert
        DebugFVal("Temp = ", curr_temp, "degC");
        if (curr_temp > PRE_HIGH_TEMP_THRESHOLD_C)   // TODO: Remove, wenn exti funktioniert
            alert_active = TRUE;      // TODO: Remove, wenn exti funktioniert

        if (alert_active)
        {

            DebugLn("======================== Hi Alert Triggered ===");
            state_transition(MODE_HIGH_TEMPERATURE);
            TMP126_CloseForAlert();
            break;
        }

        uint8_t current_pe5 = GPIO_ReadInputPin(TMP126_WAKE_PORT, TMP126_WAKE_PIN);
        rtc_get_time(&h, &m, &s);
        sprintf(buf, "[%02u:%02u:%02u] PE5 = %s", h, m, s, current_pe5 ? "HIGH" : "LOW");
        DebugLn(buf);

    //    TMP126_Format_Temperature(buf);
    //    DebugLn(buf);
    }
#ifdef PRE_HIGH_TEMP_MEASURE
    DebugLn("[PRE_HIGH_TEMP] Zyklische Messung aktiv");

    float temp = sensor_read_temperature();
    int temp_int = (int)(temp * 100); // falls gewünscht: 2 Nachkommastellen
    DebugIVal("[PRE_HIGH_TEMP] Temperatur: ", temp_int, " x0.01 C");

    if (storage_internal_add_measurement(temp))
    {
        DebugLn("[PRE_HIGH_TEMP] Temperatur gespeichert");
    }
    else
    {
        DebugLn("[PRE_HIGH_TEMP] Speicher voll oder Fehler");
    }

    state_set_mode(MODE_SLEEP);
#else
/*
    DebugLn("[PRE_HIGH_TEMP] Warte auf TMP126 Alert (EXTI)");
    TMP126_DebugWakePin();

    if (pre_hi_temp_alert_triggered)
    {
        pre_hi_temp_alert_triggered = FALSE;
        DebugLn("[PRE_HIGH_TEMP] ALERT erkannt -> Wechsel in MODE_HIGH_TEMPERATURE");
        TMP126_CloseForAlert();
        state_transition(MODE_HIGH_TEMPERATURE);
    }
    else
    {
        state_transition(MODE_SLEEP);
    }
        */
#endif
}
