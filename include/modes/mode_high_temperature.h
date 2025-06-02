#ifndef MODE_HIGH_TEMPERATURE_H
#define MODE_HIGH_TEMPERATURE_H


// Globales Flag, das vom EXTI gesetzt wird
extern volatile bool mode_hi_temp_measurement_alert_triggered;

/**
 * @brief Führt die Temperaturüberwachung im Übertemperatur-Modus durch.
 *
 * Der Modus bleibt aktiv, bis die Temperatur unter eine konfigurierte
 * Schwelle gefallen ist. Danach erfolgt eine Rückkehr in den
 * normalen OPERATIONAL-Modus.
 */
void mode_high_temperature_run(void);

#endif // MODE_HIGH_TEMPERATURE_H
