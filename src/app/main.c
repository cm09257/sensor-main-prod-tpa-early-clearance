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
#include "modes/mode_operational.h"
#include "periphery/tmp126.h"
#include "periphery/mcp7940n.h"
#include "periphery/uart.h"
#include "periphery/power.h"
#include "periphery/flash.h"
#include "periphery/RFM69.h"
#include "periphery/i2c_devices.h"
#include "periphery/spi_devices.h"
#include "periphery/system.h"
#include "modules/settings.h"
// #include "modules/storage_internal.h"
//  #include "modules/storage.h"
//  #include "modules/rtc.h"
//  #include "modules/interrupts_PCB_REV_3_1.h"
#include "utility/debug.h"
// #include "utility/debug_menu.h"
#include "utility/delay.h"
#include "utility/random.h"
#include "stm8s_clk.h"
#include "types.h"
#include "utility/u8toa.h"
#include <string.h>

////////////////////// Interrupt Service Routine (ISR)

INTERRUPT_HANDLER(EXTI_PORT_D_IRQHandler, 6) ///////////////////// RTC ISR
{
    MCP7940N_Open();
    MCP7940N_ClearAlarmFlagX(0);
    MCP7940N_ClearAlarmFlagX(1);
    MCP7940N_Close();
#if defined(DEBUG_MAIN_C)
    DebugUVal("[RTCISR]Last md=", mode_before_halt, "");
#endif
}

INTERRUPT_HANDLER(EXTI_PORT_C_IRQHandler, 5) ///////////////////// TMP126 ISR
{
    nop();
}

///////////////////// ISR end

/**
 * @brief Hauptprogramm.
 *
 * Initialisiert System und Zustand und ruft dann kontinuierlich den Dispatcher `state_process()` auf.
 * Ein optionaler Watchdog könnte in zukünftigen Phasen hinzugefügt werden.
 */
void main(void)
{
    system_init_phase_1(); ///< Systemkomponenten initialisieren

    // TODO: For Debug -> remove
    settings_set_default();
    settings_save();
    ///////////////////////////////

    settings_load();
    settings_t *settings = settings_get();

    bool do_chip_erase = FALSE;
    if (!(settings->flags & SETTINGS_FLAG_FLASH_ERASE_DONE))
    {
#if defined(DEBUG_MAIN_C)
        DebugLn("Erasing flash...");
#endif
        do_chip_erase = TRUE;
        settings->flags |= SETTINGS_FLAG_FLASH_ERASE_DONE;
        settings->flash_record_count = 0;
        settings_save();
    }
    else
    {
#if defined(DEBUG_MAIN_C)
        DebugLn("No erase required");
#endif
        nop();
    }
    system_init_phase_2(do_chip_erase, settings->offset_hz);
#if defined(DEBUG_MAIN_C)
    DebugLn("=Sensor Main=");
#endif
    DebugLn("TPA Early Clearance Variant");
    DebugHex("Sensor ID MSB ", DEVICE_ID_MSB);
    DebugHex("Sensor ID LSB ", DEVICE_ID_LSB);

    state_init(); ///< Zustandsmaschine aus EEPROM laden oder auf MODE_TEST setzen
                  // DebugMenu_Init(); // Show Debug Menu

    // DebugLn("[sensor-main] Going into mode MODE_PRE_HIGH_TEMP");
    set_mode_debug_only(MODE_TEST);
    // DebugLn("[sensor-main] MODE_PRE_HIGH_TEMP set.");
    while (1)
    {
        // DebugLn("[sensor-main] In main while-loop.");
        state_process();
        // DebugMenu_Update();
    }
}
