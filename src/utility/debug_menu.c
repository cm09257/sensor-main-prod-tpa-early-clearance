#include <stdio.h>
#include "stm8s_uart1.h"
#include "app/state_machine.h"
#include "common/types.h" // für mode_name(), MODE_COUNT
#include "modules/rtc.h"
#include "periphery/tmp126.h"
#include "periphery/uart.h"
#include "utility/debug.h"
#include "utility/debug_menu.h"

static char rx_buffer;

void DebugMenu_Init(void)
{
    DebugLn("[DEBUG MENU] Possible Commands:");
    DebugLn("  t = Temperature Readout");
    DebugLn("  r = RTC Time Readout");
    DebugLn("  0-9 = Set Mode");

    DebugLn("  Available Modes:");
    for (uint8_t i = 0; i < MODE_COUNT; i++)
    {
        DebugUVal("     ", i, mode_name((mode_t)i));
    }

    // Aktueller Modus anzeigen
    DebugLn("[Current Mode]:");
    DebugLn(mode_name(state_get_current()));
}

bool DebugMenu_Update(void)
{
    static char rx_buffer;

    if (!UART1_ReceiveByteNonBlocking(&rx_buffer))
        return FALSE;

    switch (rx_buffer)
    {
    case 't':
    {
        char buf[32];
        TMP126_OpenForMeasurement();
        TMP126_Format_Temperature(buf);
        TMP126_CloseForMeasurement();
        DebugLn(buf);
        break;
    }
    case 'r':
    {
       
        char buf[32];
        rtc_get_format_time(buf);
        DebugLn(buf);
        break;
    }
    case 'n':
    {
        DebugLn("[DEBUG MENU] 'n' received, leaving Debug Menu.");
        return TRUE;
    }

    default:
    {
        if (rx_buffer >= '0' && rx_buffer <= '9')
        {
            uint8_t mode = rx_buffer - '0';
            if (mode < MODE_COUNT)
            {
                
                DebugLn("[Manuell] Setze Modus auf MODE:");
                DebugLn(mode_name((mode_t)mode));
                
                state_transition((mode_t)mode);
            }
            else
            {
                DebugLn("[Fehler] Ungültiger Modus");
            }
        }
        else
        {
            Debug("[DEBUG MENU] Unbekanntes Kommando: ");
            char msg[2] = {rx_buffer, '\0'};
            Debug(msg);
            DebugLn("");
        }
        break;
    }
    }

    return FALSE;
}
