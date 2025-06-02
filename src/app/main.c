/**
 * @file main.c
 * @brief Einstiegspunkt für das Sensorgerät (STM8AF5288)
 *
 * Diese Datei initialisiert alle Systemkomponenten (UART, SPI, RTC, Sensor, Funk, Settings)
 * und startet anschließend die Zustandsmaschine (state machine).
 *
 * Der Haupt-Loop ruft kontinuierlich `state_process()` auf, um je nach Modus
 * (`MODE_OPERATIONAL`, `MODE_HIGH_TEMPERATURE`, etc.) den Gerätezustand abzuarbeiten.
 */
#include <stdio.h>
#include "app/state_machine.h"
#include "modes/mode_pre_high_temperature.h"
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/uart.h"
#include "periphery/power.h"
#include "periphery/flash.h"
#include "periphery/i2c_devices.h"
#include "modules/radio.h"
#include "modules/settings.h"
#include "modules/storage_internal.h"
#include "modules/storage.h"
#include "modules/rtc.h"
#include "modules/interrupts_PCB_REV_3_1.h"
#include "utility/debug.h"
#include "utility/debug_menu.h"
#include "utility/delay.h"
#include "utility/random.h"
#include "stm8s_clk.h"


INTERRUPT_HANDLER(EXTI_PORT_D_IRQHandler, PORT_D_INTERRUPT_VECTOR) // RTC_WAKE = PD2
{
    MCP7940N_DisableAlarmX(1);  // immediately disable and clear alarm
    MCP7940N_ClearAlarmFlagX(1); // in ISR
    char buf[64];
    sprintf(buf, "PD2 Interrupt ausgeloest");
    DebugLn(buf);
    MCP7940N_SetTime(0, 0, 55);
    MCP7940N_ConfigureAbsoluteAlarmX(1, 0, 1, 0); // Alarm bei sek = 00:01:00
    MCP7940N_EnableAlarmX(1);
}

INTERRUPT_HANDLER(EXTI_PORT_C_IRQHandler, PORT_C_INTERRUPT_VECTOR) // TMP126 ALERT = PE5
{   
    pre_hi_temp_alert_triggered = TRUE;
}

/**
 * @brief Initialisiert alle Systemkomponenten.
 *
 * Diese Funktion führt die vollständige Initialisierung der Hardware und
 * Konfigurationsmodule durch. Sie sollte **vor** `state_init()` aufgerufen werden.
 *
 * Komponenten:
 * - UART (für Debug-Ausgabe)
 * - Zufallsgenerator (für Funk-Retry-Zeiten)
 * - SPI-Peripherie (Sensor, Flash, Funkmodul)
 * - RTC
 * - Sensor (TMP126)
 * - Funkmodul (RFM69)
 * - Persistente Einstellungen
 */
void system_init(void)
{
    CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV8); // 2 MHz
    CLK_PeripheralClockConfig(CLK_PERIPHERAL_I2C, ENABLE);
    UART1_MyInit(); ///< Debug UART konfigurieren (9600 8N1)
    I2C_Devices_Init();
    TMP126_init(); // TMP126 initialisieren.
    delay(100);
    MCP7940N_Init(); ///< Realtime Clock initialisieren
    
    global_power_save();
    delay(100);

    random_seed(0x1234); ///< Seed für Zufallsfunktionen setzen

    Flash_Init();
    internal_storage_init();
    // settings_set_default();
    // settings_save();
    persist_current_mode(MODE_TEST);

    settings_load(); //  ///< Geräteeinstellungen (EEPROM) laden
    radio_init();    ///< RFM69 Funkmodul vorbereiten (z. B. Standby)
}

/**
 * @brief Hauptprogramm.
 *
 * Initialisiert System und Zustand und ruft dann kontinuierlich den Dispatcher `state_process()` auf.
 * Ein optionaler Watchdog könnte in zukünftigen Phasen hinzugefügt werden.
 */
void main(void)
{
    system_init(); ///< Systemkomponenten initialisieren
    DebugLn("[sensor-main] Finished system_init()");

    state_init(); ///< Zustandsmaschine aus EEPROM laden oder auf MODE_TEST setzen
                  // DebugMenu_Init(); // Show Debug Menu

    // DebugLn("[sensor-main] Going into mode MODE_PRE_HIGH_TEMP");
    set_mode_debug_only(MODE_PRE_HIGH_TEMP);
    // DebugLn("[sensor-main] MODE_PRE_HIGH_TEMP set.");
    while (1)
    {
        //       DebugLn("[sensor-main] In main while-loop.");
        state_process();
        // DebugMenu_Update();
    }
    /*
        char buf[20];
        while (1)
        {
            uint8_t hour, min, sec;
            MCP7940N_GetTime(&hour, &min, &sec);
            DebugUVal("h:", hour, "");
            DebugUVal("m:", min, "");
            DebugUVal("s:", sec, "");

            TMP126_OpenForMeasurement();
            TMP126_Format_Temperature(buf);
            TMP126_CloseForMeasurement();
            DebugLn(buf);

            delay(5000); // 5 Sekunden
        }*/
}
