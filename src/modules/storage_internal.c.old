#include "modules/storage_internal.h"
#include "stm8s_flash.h"
#include "utility/debug.h"

// == Für STM8AF5288: EEPROM liegt bei 0x004000..0x0047FF (2kB) ==
// Wir reservieren nur 1kB: 0x004000..0x0043FF
#define INTERNAL_EEPROM_START    0x004000UL
#define INTERNAL_EEPROM_SIZE     1024U
// 5 Byte pro Datensatz → 1024 / 5 = 204 Datensätze (34h bei 10min Intervall)

static uint16_t internal_write_offset = 0; // Byte-Offset im reservierten EEPROM

void internal_storage_init(void)
{
    FLASH_Unlock(FLASH_MEMTYPE_DATA);
    // optional: internal_flash_erase_all();
}

bool internal_flash_write_record(const record_t* rec)
{
    if (!rec) return FALSE;

    if (internal_write_offset + 5 > INTERNAL_EEPROM_SIZE) {
        DebugLn("[internal] EEPROM voll! Kein Platz mehr.");
        return FALSE;
    }

    uint8_t raw[5];
    uint16_t ts16 = (uint16_t)(rec->timestamp & 0xFFFF);
    int16_t temp_fixed = (int16_t)(rec->temperature * 100);

    raw[0] = ts16 & 0xFF;
    raw[1] = (ts16 >> 8) & 0xFF;
    raw[2] = temp_fixed & 0xFF;
    raw[3] = (temp_fixed >> 8) & 0xFF;
    raw[4] = rec->flags;

    uint32_t addr = INTERNAL_EEPROM_START + internal_write_offset;
    for (uint8_t i = 0; i < 5; i++) {
        FLASH_ProgramByte(addr + i, raw[i]);
    }

   // DebugUVal("[internal] Write: Timestamp", ts16, "");
   // DebugUVal("           Temp x100", temp_fixed, "");
   // DebugUVal("           Flags", rec->flags, "");

    internal_write_offset += 5;
    return TRUE;
}

bool internal_flash_read_record(uint16_t index, record_t* out)
{
    if (!out) return FALSE;

    if (index >= (INTERNAL_EEPROM_SIZE / 5)) return FALSE;

    uint32_t addr = INTERNAL_EEPROM_START + (uint32_t)index * 5;

    static uint8_t raw[5]; // statisch statt Stack

    // Zugriff nur zur Laufzeit
    for (uint8_t i = 0; i < 5; i++) {
        raw[i] = FLASH_ReadByte(addr + i);
    }

    uint16_t ts16 = (raw[1] << 8) | raw[0];
    int16_t temp_fixed = (raw[3] << 8) | raw[2];

    out->timestamp = ts16;
    out->temperature = temp_fixed / 100.0f;
    out->flags = raw[4];

    return TRUE;
}



uint16_t internal_flash_get_count(void)
{
    return internal_write_offset / 5;
}

void internal_flash_erase_all(void)
{
    DebugLn("[internal] EEPROM-Erase gestartet...");
    for (uint16_t offset = 0; offset < INTERNAL_EEPROM_SIZE; offset += 64)
    {
        uint32_t block_addr = (INTERNAL_EEPROM_START + offset);
        FLASH_EraseBlock(block_addr / 64, FLASH_MEMTYPE_DATA);
    }
    internal_write_offset = 0;
    DebugLn("[internal] EEPROM gelöscht und Offset zurückgesetzt.");
}
