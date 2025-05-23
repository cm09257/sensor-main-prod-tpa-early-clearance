// mode_test.c
#include "modes/mode_test.h"
#include "app/state_machine.h"
#include "utility/debug.h"
#include "utility/delay.h"

void mode_test_run(void) {
    DebugLn("=== MODE_TEST ===");
    DebugLn("[TEST] Starte Initial-Selbsttest (Dummy-Version)");
    DebugLn("[TEST] Warte 5 Sekunden vor Übergang zu MODE_WAIT_FOR_ACTIVATION");

    delay(5000);  // Wartezeit in Millisekunden

    DebugLn("[TEST] Übergang zu MODE_WAIT_FOR_ACTIVATION");
    state_transition(MODE_WAIT_FOR_ACTIVATION);
}
