#ifndef MODE_WAIT_FOR_ACTIVATION_H
#define MODE_WAIT_FOR_ACTIVATION_H

/**
 * @brief Führt den Modus WAIT_FOR_ACTIVATION aus.
 *
 * In diesem Modus sendet der Sensor regelmäßig ein Ping an das Gateway
 * und wartet auf einen spezifischen Aktivierungsbefehl (z. B. Downlink mit 0xB0...09).
 *
 * Wenn eine gültige Aktivierung empfangen wurde, erfolgt ein Zustandswechsel
 * in MODE_HIGH_TEMPERATURE.
 *
 * Der Modus nutzt RTC-basierte Wakeups und minimiert den Energieverbrauch zwischen den Versuchen.
 */
void mode_wait_for_activation_run(void);

#endif // MODE_WAIT_FOR_ACTIVATION_H
