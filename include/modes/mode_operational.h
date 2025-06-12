#ifndef MODE_OPERATIONAL_H
#define MODE_OPERATIONAL_H

#include "common/types.h"

extern volatile bool mode_operational_rtc_alert_triggered;

/**
 * @brief Führt den normalen Betriebsmodus aus (MODE_OPERATIONAL).
 *
 * In diesem Modus wird regelmäßig eine Temperaturmessung durchgeführt,
 * das Ergebnis im Flash gespeichert und anhand eines Zählers entschieden,
 * ob ein Datentransfer (Funkübertragung) erfolgen soll.
 *
 * Der Modus ist nach einer
 * Abkühlphase (HIGH_TEMPERATURE) aktiv.
 */
void mode_operational_run(void);

#endif // MODE_OPERATIONAL_H
