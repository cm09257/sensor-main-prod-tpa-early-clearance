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
#include "modes/mode_high_temperature.h"
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/uart.h"
#include "periphery/power.h"
#include "periphery/flash.h"
#include "periphery/i2c_devices.h"
#include "periphery/spi_devices.h"
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
#include "common/types.h"
#include "utility/u8toa.h"
#include <string.h>

INTERRUPT_HANDLER(EXTI_PORT_D_IRQHandler, PORT_D_INTERRUPT_VECTOR) // RTC_WAKE = PD2
{
    DebugUVal("[ISR] Last mode = ", mode_before_halt, "");
    if (mode_before_halt == MODE_HIGH_TEMPERATURE)
    {
        DebugLn("[ISR] Last mode = MODE_HIGH_TEMPERATURE");
        mode_hi_temp_measurement_alert_triggered = TRUE;
    }
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
    UART1_MyInit(); ///< Debug UART konfigurieren (9600 8N1)
    DebugLn("=================== SENSOR MAIN");
    DebugLn("[system_init]");
    global_power_save();
    CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV8); // 2 MHz
    CLK_PeripheralClockConfig(CLK_PERIPHERAL_I2C, ENABLE);

    I2C_Devices_Init();
    TMP126_init(); // TMP126 initialisieren.
    delay(100);
    MCP7940N_Init(); ///< Realtime Clock initialisieren
    delay(100);

    random_seed(0x1234); ///< Seed für Zufallsfunktionen setzen

    Flash_Init();
    internal_storage_init();
    // settings_set_default();
    // settings_save();
    //   persist_current_mode(MODE_TEST);

    //  settings_load(); //  ///< Geräteeinstellungen (EEPROM) laden
    //  radio_init();    ///< RFM69 Funkmodul vorbereiten (z. B. Standby)
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

    Flash_Open();
    DebugLn("Flash open");

   // uint8_t mfg8, type8, capacity8;
    uint16_t mfg, type, capacity;

    spi_flash_select();
    spi_flash_transfer(0x9F); // JEDEC ID command
    mfg = spi_flash_transfer(0x00);
    type = spi_flash_transfer(0x00);
    capacity = spi_flash_transfer(0x00);
    spi_flash_unselect();

    DebugUVal("mfg = ",mfg,"x");
  //  mfg = (uint16_t)mfg8;
  //  type = (uint16_t)type8;
  //  capacity = (uint16_t)capacity8;
    DebugLn("done");

 //   char valstr[8];
 //   strcpy(valstr, "penil");
    
   // u8toa(mfg, valstr);
   // UART1_SendString("mfg = ");
   // UART1_SendString(valstr);
  //  UART1_SendString(unit);
 //   UART1_SendString("\r\n");

    /*    DebugUVal("TYPE=",type,"");
        DebugUVal("CAP=", capacity,"");
    */
    while (1)
    {
        nop();
    }
    /*
        // storage_flash_test();

        uint8_t count = flash_get_count();
        DebugUVal(" Flash Record Count ", count, "");

        record_t rec;
        rec.timestamp = 5;
        rec.temperature = 21.3f;
        rec.flags = 1;

        if (flash_write_record_nolock(&rec))
            DebugLn("flash write ok.");
        else
            DebugLn("flash write not ok");

        count = flash_get_count();
        DebugUVal(" Flash Record Count ", count, "");

        delay(1000);

        record_t read_record;
        flash_read_record(0, &read_record);
        uint8_t ts = read_record.timestamp;
        float te = read_record.temperature;
        uint8_t fl = read_record.flags;

        DebugLn("[Flash] Lese Datensatz aus Flash");
        DebugUVal("-> Timestamp   = ", ts, "");
        DebugFVal("-> Temperature = ", te, "");
        DebugUVal("-> Flags       = ", fl, "");

        while (1)
        {
            nop();
        }

    */
    /*
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

     */
}
