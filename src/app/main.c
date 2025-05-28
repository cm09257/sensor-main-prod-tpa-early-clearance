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
    MCP7940N_SetTime(0, 0, 0);

    random_seed(0x1234); ///< Seed für Zufallsfunktionen setzen
                         // radio_init();        ///< RFM69 Funkmodul vorbereiten (z. B. Standby)
    Flash_Init();
    internal_storage_init();    
    settings_load();

    DebugLn("Now trying Radio Init");
    radio_init();
    DebugLn("Radio Init successful");
    

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

    /* CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV8); // 2 MHz
     CLK_PeripheralClockConfig(CLK_PERIPHERAL_I2C, ENABLE);
     UART1_MyInit();
     // SPI1_Init_Hard();
     // SPI_Devices_Init();
     I2C_Devices_Init();
     DebugLn("Periphery Test - Init done...");

     MCP7940N_Init();
     delay(100);
     DebugLn("MCP7940 Init complete");

     while (1)
     {
         delay(100);
     }*/

    system_init(); ///< Systemkomponenten initialisieren
                   /*  state_init();  ///< Zustandsmaschine aus EEPROM laden oder auf MODE_TEST setzen
               
                     while (1)
                     {
                         // watchdog_feed();    ///< TODO: Watchdog regelmäßig zurücksetzen
                         state_process(); ///< Dispatcher-Funktion für aktuellen Modus
                     }*/

    DebugLn("Finished system_init()");
    char tbuf[64];
    TMP126_OpenForMeasurement();
    delay(100);
    TMP126_Format_Temperature(tbuf);
    TMP126_CloseForMeasurement();
    DebugLn(tbuf);

    while (1)
    {
        delay(100);
    }
}
