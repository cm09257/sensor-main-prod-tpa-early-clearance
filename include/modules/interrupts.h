#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "common/types.h"

/**
 * @file interrupts.h
 * @brief Deklaration von Interrupt-Service-Routinen (ISR) für das System.
 *
 * Diese Datei enthält die Prototypen für benutzerdefinierte Interrupts,
 * insbesondere für externe Interrupts über die RTC.
 */

// Optional: Hier könnten weitere ISR-Prototypen ergänzt werden,
// falls z. B. Timer, ADC oder USART Interrupts verwendet werden.

/**
 * @brief EXTI0 Interrupt Handler – wird ausgelöst durch ein externes Ereignis
 * an Port A, Pin 1 (PA1), typischerweise vom RTC INT-Ausgang.
 *
 * In der ISR wird der Weckgrund verarbeitet (z. B. aktueller Modus erkannt)
 * und ggf. ein Zustandswechsel eingeleitet.
 */
INTERRUPT_HANDLER(EXTI0_IRQHandler, 3);

#endif // INTERRUPTS_H
