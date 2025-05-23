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
 #include "periphery/spi_devices.h"
 #include "periphery/uart.h"
 #include "modules/sensor.h"
 #include "modules/radio.h"
 #include "modules/rtc.h"
 #include "modules/settings.h"
 #include "utility/debug.h"
 #include "utility/random.h"
 
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
     UART1_Init();               ///< Debug UART konfigurieren (9600 8N1)
     DebugLn("System init...");
 
     random_seed(0x1234);        ///< Seed für Zufallsfunktionen setzen
     SPI_Devices_Init();         ///< SPI-Bus initialisieren (Sensor, Flash, Funk)
     rtc_init();                 ///< Realtime Clock initialisieren
 
     sensor_init();              ///< Temperatur-Sensor (TMP126) initialisieren
     radio_init();               ///< RFM69 Funkmodul vorbereiten (z. B. Standby)
     settings_load();            ///< Geräteeinstellungen (EEPROM) laden
 
     DebugLn("System init abgeschlossen");
 }
 
 /**
  * @brief Hauptprogramm.
  *
  * Initialisiert System und Zustand und ruft dann kontinuierlich den Dispatcher `state_process()` auf.
  * Ein optionaler Watchdog könnte in zukünftigen Phasen hinzugefügt werden.
  */
 int main(void)
 {
     // watchdog_init();        ///< TODO: Watchdog initialisieren (AWU-basiert), Phase 2
     system_init();             ///< Systemkomponenten initialisieren
     state_init();              ///< Zustandsmaschine aus EEPROM laden oder auf MODE_TEST setzen
 
     while (1)
     {
         // watchdog_feed();    ///< TODO: Watchdog regelmäßig zurücksetzen
         state_process();       ///< Dispatcher-Funktion für aktuellen Modus
     }
 }
 