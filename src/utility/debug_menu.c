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
    DebugLn("[DEBUG MENU] Aktiv. Befehle:");
    DebugLn("  t = Temperatur lesen");
    DebugLn("  r = RTC-Zeit anzeigen");
    DebugLn("  0-9 = Modus setzen");

    DebugLn("  Verfügbare Modi:");
    for (uint8_t i = 0; i < MODE_COUNT; i++)
    {
        char buf[48];
        sprintf(buf, "    %u = MODE_%s", i, mode_name((mode_t)i));
        DebugLn(buf);
    }

    // Aktueller Modus anzeigen
    char buf[48];
    sprintf(buf, "[Aktuell] MODE_%s", mode_name(state_get_current()));
    DebugLn(buf);
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
        uint8_t h, m, s;
        rtc_get_time(&h, &m, &s);
        char buf[32];
        sprintf(buf, "[RTC] %02u:%02u:%02u", h, m, s);
        DebugLn(buf);
        break;
    }
    case 'n':
        DebugLn("[DEBUG MENU] 'n' erkannt, verlasse Menü.");
        return TRUE;

    default:
        if (rx_buffer >= '0' && rx_buffer <= '9')
        {
            uint8_t mode = rx_buffer - '0';
            if (mode < MODE_COUNT)
            {
                char buf[48];
                sprintf(buf, "[Manuell] Setze Modus auf MODE_%s", mode_name((mode_t)mode));
                DebugLn(buf);
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

    return FALSE;
}
