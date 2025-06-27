// mode_test.c
#include "modes/mode_test.h"
#include "app/state_machine.h"
#include "config/config.h"
#include "utility/debug.h"
//#include "utility/debug_menu.h"
#include "utility/delay.h"

void mode_test_run(void)
{
#if defined(DEBUG_MODE_TEST)
    DebugLn("=============== MODE_TEST ===============");
    DebugLn("[MODE_TEST] Starting initial self test (dummy version).");
    DebugLn("[MODE_TEST] Wait 5sec before switching to MODE_WAIT_FOR_ACTIVATION.");
#endif

    delay(500); // Wartezeit in Millisekunden
#if defined(DEBUG_MODE_TEST)
    DebugLn("[MODE_TEST] Switching to MODE_WAIT_FOR_ACTIVATION.");
#endif

    state_transition(MODE_WAIT_FOR_ACTIVATION);
}
