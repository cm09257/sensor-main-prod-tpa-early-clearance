// include/modes/mode_pre_high_temperature.h
#ifndef MODE_PRE_HIGH_TEMPERATURE_H
#define MODE_PRE_HIGH_TEMPERATURE_H

#include "stm8s.h"

// Globales Flag, das vom EXTI gesetzt wird
extern volatile bool pre_hi_temp_alert_triggered;

// Hauptroutine des Zustands
void mode_pre_high_temperature_run(void);

#endif // MODE_PRE_HIGH_TEMPERATURE_H
