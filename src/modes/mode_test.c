// mode_test.c
#include "modes/mode_test.h"
#include "app/state_machine.h"
#include "config/config.h"
#include "utility/debug.h"
#include "utility/debug_menu.h"
#include "utility/delay.h"

void mode_test_run(void)
{
    DebugLn("=============== MODE_TEST ===============");
    DebugLn("[MODE_TEST] Starting initial self test (dummy version).");
    DebugLn("[MODE_TEST] Wait 5sec before switching to MODE_WAIT_FOR_ACTIVATION.");

    delay(5000); // Wartezeit in Millisekunden

    DebugLn("[MODE_TEST] Switching to MODE_WAIT_FOR_ACTIVATION.");
#if defined DEV_MODE
    // DebugMenu_Init();
    // DebugLn("[DEBUG MENU] Warten auf Eingabe 'n' für nächsten Modus...");

    /*    while (1)
        {
            if (DebugMenu_Update())
            {
                DebugLn("[DEBUG MENU] Verlasse Debug-Menü...");
                break;
            }
            delay(100); // CPU entlasten
        }*/

#else
    state_transition(MODE_WAIT_FOR_ACTIVATION);
#endif
    state_transition(MODE_WAIT_FOR_ACTIVATION);
}
