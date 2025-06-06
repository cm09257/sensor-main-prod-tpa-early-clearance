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

    uint16_t mfg, type, capacity;

    spi_flash_select();
    spi_flash_transfer(0x9F); // JEDEC ID command
    mfg = spi_flash_transfer(0x00);
    type = spi_flash_transfer(0x00);
    capacity = spi_flash_transfer(0x00);
    spi_flash_unselect();

    DebugHex16("mfg      = ", mfg);
    DebugHex16("type     = ", type);
    DebugHex16("capacity =", capacity);

    // storage_flash_test();

    // uint8_t count = flash_get_count();
    // DebugUVal(" Flash Record Count ", count, "");

    record_t rec;
    rec.timestamp = 5;
    rec.temperature = 25.3f;
    rec.flags = 1;

    uint8_t record_data[6];

    // Timestamp (4 Byte, little endian)
    record_data[0] = (uint8_t)(rec.timestamp & 0xFF);
    record_data[1] = (uint8_t)((rec.timestamp >> 8) & 0xFF);
    record_data[2] = (uint8_t)((rec.timestamp >> 16) & 0xFF);
    record_data[3] = (uint8_t)((rec.timestamp >> 24) & 0xFF);

    // Temperatur mit Offset –50 °C (1 Byte)
    record_data[4] = (uint8_t)(rec.temperature + 50.0f + 0.5f);

    // Flags (1 Byte)
    record_data[5] = rec.flags;

    Flash_PageProgram(0x0000, record_data, 6);

    DebugLn("Flash written");

    uint8_t read[6];
    // Flash_ReadData(0x0000, read, 6);

    read[0] = 0x11;
    read[1] = 0x22;
    read[2] = 0x33;
    read[3] = 0x44;
    read[4] = 0x55;
    read[5] = 0x66;

    uint8_t read0, read1, read2, read3, read4, read5;
    read0 = read[0];
    read1 = read[1];
    read2 = read[2];
    read3 = read[3];
    read4 = read[4];
    read5 = read[5];

    DebugHex("read[0] =", read0);
  //  DebugHex("read[1] =", read1);
    //DebugHex("read[2] =", read2);
//    DebugHex("read[3] =", read3);
 //     DebugHex("read[4] =", read4);
 //     DebugHex("read[5] =", read5);

    DebugUVal("temp ", read[4] - 50, "degC");

    //   count = flash_get_count();
    //  DebugUVal(" Flash Record Count ", count, "");

    delay(1000);

    //  uint8_t record_data[5];

    // record_t read_record;
    //  flash_read_record(0, &read_record);
    /*    uint8_t ts = read_record.timestamp;
        float te = read_record.temperature;
        uint8_t fl = read_record.flags;

        DebugLn("[Flash] Lese Datensatz aus Flash");
        DebugUVal("-> Timestamp   = ", ts, "");
        DebugFVal("-> Temperature = ", te, "");
        DebugUVal("-> Flags       = ", fl, "");
    */
    while (1)
    {
        nop();
    }

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
