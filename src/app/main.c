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
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/uart.h"
#include "periphery/power.h"
#include "periphery/flash.h"
#include "periphery/i2c_devices.h"
#include "modules/radio.h"
#include "modules/settings.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "utility/random.h"
#include "stm8s_clk.h"

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
    DebugLn("System init...");

    DebugLn("TMP126_init...");
    TMP126_init(); // TMP126 initialisieren.
    DebugLn("TMP126_init done...");
    delay(100);
    global_power_save();

    MCP7940N_Init(); ///< Realtime Clock initialisieren
    delay(100);

    random_seed(0x1234); ///< Seed für Zufallsfunktionen setzen
                         // radio_init();        ///< RFM69 Funkmodul vorbereiten (z. B. Standby)
    Flash_Init();
    internal_storage_init();
    settings_load();
    radio_init();

    // TODO: BUG: RFM69 resets in RFM69_PowerUp.

    //  ///< Geräteeinstellungen (EEPROM) laden
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
 //  state_init();  ///< Zustandsmaschine aus EEPROM laden oder auf MODE_TEST setzen

    DebugLn("Finished system_init()");

    char buf[20];
    while (1)
    {
        uint8_t hour, min, sec;
        MCP7940N_GetTime(&hour, &min, &sec);
        DebugUVal("h:",hour,"");
        DebugUVal("m:",min,"");
        DebugUVal("s:",sec,"");

        TMP126_OpenForMeasurement();
        TMP126_Format_Temperature(buf);
        TMP126_CloseForMeasurement();
        DebugLn(buf);
        
        delay(5000); // 5 Sekunden
    }
}
