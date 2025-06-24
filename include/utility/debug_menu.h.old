// debug_menu.h
#ifndef DEBUG_MENU_H
#define DEBUG_MENU_H

/**
 * @file debug_menu.h
 * @brief UART-Debug-Menü zur manuellen Steuerung von Sensor, RTC und Modi
 *
 * Aktivierbar unabhängig von DEBUG-Ausgabe – eignet sich für Entwicklung und Test.
 */

/**
 * @brief Initialisiert das Debug-Menü (UART-Setup, Hinweistext).
 */
void DebugMenu_Init(void);

/**
 * @brief Zyklisch im Mainloop aufrufen. Erkennt Kommandos und führt Aktionen aus.
 */
bool DebugMenu_Update(void);

#endif // DEBUG_MENU_H