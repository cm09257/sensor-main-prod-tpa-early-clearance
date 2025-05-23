#ifndef MODE_TEST_H
#define MODE_TEST_H

/**
 * @brief Führt den TEST-Modus aus.
 *
 * Dieser Modus ist für Entwicklungs- und Testzwecke vorgesehen.
 * Er erlaubt z. B. manuelles Triggern von Messungen, 
 * Debug-Ausgaben oder kontrolliertes Verhalten zum Validieren der Peripherie.
 *
 * Die genaue Funktion hängt von der Implementierung in `mode_test.c` ab.
 */
void mode_test_run(void);

#endif // MODE_TEST_H
