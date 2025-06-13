#include "modules/storage.h"
#include "modules/storage_internal.h"
#include "common/types.h"
#include "periphery/flash.h"
#include "utility/debug.h"
#include "utility/delay.h"
#include "stm8s_flash.h"
#include <string.h>
#include "periphery/uart.h"

#define EEPROM_ADDR_MODE 0x10
#define EEPROM_ADDR_DEVICE_ID 0x20

#define DEVICE_ID_MAGIC 0xA5
#define DEVICE_ID_LENGTH 4
#define DEVICE_ID_TOTAL_SIZE (1 + DEVICE_ID_LENGTH)

#define FLASH_ADDR_BASE 0x000100UL
#define RECORD_SIZE_BYTES 5
#define MAX_RECORDS 2048

static uint32_t write_ptr = 0;

//////// Helper Functions

static uint8_t calc_crc8(uint8_t value)
{
    uint8_t crc = 0;
    crc ^= value;
    for (uint8_t i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x1D : (crc << 1);
    return crc;
}

static uint8_t crc4_timestamp(uint32_t ts_5min)
{
    uint8_t crc = 0;
    crc ^= (ts_5min >> 0) & 0xFF;
    crc ^= (ts_5min >> 8) & 0xFF;
    crc ^= (ts_5min >> 16) & 0x0F;
    for (uint8_t i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x03 : (crc << 1);
    return crc & 0x0F;
}

//////// Internal Flash

void storage_eeprom_unlock(void) // internal flash
{
    DebugLn("[storage] EEPROM entsperrt");
    FLASH_Unlock(FLASH_MEMTYPE_DATA);
}

void storage_write_eeprom(uint16_t address, const uint8_t *data, uint16_t len) // internal flash
{
    /*
    // DebugLn("Manual EEPROM Unlock Test");

    FLASH->CR2 |= FLASH_CR2_PRG;
    FLASH->DUKR = 0xAE;
    FLASH->DUKR = 0x56;

    if (FLASH->IAPSR & FLASH_IAPSR_DUL)
    {
        // DebugLn("EEPROM Unlock erfolgreich!");
        delay(1);
    }
    else
    {
        DebugLn("EEPROM Unlock FEHLGESCHLAGEN!");
    }*/

    // EEPROM entsperren, falls nicht bereits passiert
    if (!(FLASH->IAPSR & FLASH_IAPSR_DUL))
    {
        FLASH_Unlock(FLASH_MEMTYPE_DATA);
        DebugLn("[EEPROM] Unlock done.");
    }

    for (uint16_t i = 0; i < len; ++i)
    {
        uint32_t target = FLASH_DATA_START_PHYSICAL_ADDRESS + address + i;
        if (target > (FLASH_DATA_START_PHYSICAL_ADDRESS + 0x3FF))
        {
            DebugLn("[EEPROM] Schreibversuch außerhalb gültiger Adresse!");
            break;
        }

        FLASH_ProgramByte(target, data[i]);

        // auf Abschluss warten
        while (!(FLASH->IAPSR & FLASH_IAPSR_EOP))
            ;
    }
}

bool storage_read_eeprom(uint16_t address, uint8_t *data, uint16_t len) // internal flash
{
    for (uint16_t i = 0; i < len; ++i)
    {
        data[i] = *(uint8_t *)(FLASH_DATA_START_PHYSICAL_ADDRESS + address + i);
    }
    return TRUE;
}

void persist_current_mode(mode_t mode) // internal flash
{
    // DebugUVal("[storage] Speichere Modus:", mode, "");
    uint8_t value = (uint8_t)mode;
    uint8_t crc = calc_crc8(value);
    storage_write_eeprom(EEPROM_ADDR_MODE, &value, 1);
    storage_write_eeprom(EEPROM_ADDR_MODE + 1, &crc, 1);
}

bool load_persisted_mode(mode_t *out_mode) // internal flash
{
    uint8_t value, crc;
    storage_read_eeprom(EEPROM_ADDR_MODE, &value, 1);
    storage_read_eeprom(EEPROM_ADDR_MODE + 1, &crc, 1);
    if (calc_crc8(value) == crc)
    {
        *out_mode = (mode_t)value;
        //  DebugUVal("[storage] Geladener Modus:", value, "");
        return TRUE;
    }
    DebugLn("[storage] CRC Fehler beim Laden des Modus");
    return FALSE;
}

/*bool load_device_id(uint8_t *out) // internal flash
{
    uint8_t buf[DEVICE_ID_TOTAL_SIZE];
    if (!storage_read_eeprom(EEPROM_ADDR_DEVICE_ID, buf, DEVICE_ID_TOTAL_SIZE))
    {
        DebugLn("[storage] Fehler beim Lesen der Geräte-ID");
        return FALSE;
    }
    if (buf[0] != DEVICE_ID_MAGIC)
    {
        DebugLn("[storage] Geräte-ID Magic Byte ungültig");
        return FALSE;
    }
    memcpy(out, &buf[1], DEVICE_ID_LENGTH);
    DebugLn("[storage] Geräte-ID geladen");
    return TRUE;
}*/

/*bool store_device_id(const uint8_t *id) // internal flash
{
    uint8_t check[1];
    if (!storage_read_eeprom(EEPROM_ADDR_DEVICE_ID, check, 1))
    {
        DebugLn("[storage] Geräte-ID Prüfung fehlgeschlagen");
        return FALSE;
    }
    if (check[0] == DEVICE_ID_MAGIC)
    {
        DebugLn("[storage] Geräte-ID bereits gesetzt : kein Überschreiben erlaubt");
        return FALSE;
    }
    uint8_t buf[DEVICE_ID_TOTAL_SIZE];
    buf[0] = DEVICE_ID_MAGIC;
    memcpy(&buf[1], id, DEVICE_ID_LENGTH);
    storage_write_eeprom(EEPROM_ADDR_DEVICE_ID, buf, DEVICE_ID_TOTAL_SIZE);
    DebugLn("[storage] Neue Geräte-ID gespeichert");
    return TRUE;
}*/

//////// External Flash

bool flash_write_record(const record_t *rec) // external flash
{
    if (!rec)
        return FALSE;

    if (!Flash_Open())
    {
        DebugLn("[Flash] Öffnen fehlgeschlagen");
        return FALSE;
    }

    uint8_t raw[RECORD_SIZE_BYTES];
    uint32_t ts = rec->timestamp;
    uint8_t crc4 = crc4_timestamp(ts);
    raw[0] = (ts >> 0) & 0xFF;
    raw[1] = (ts >> 8) & 0xFF;
    raw[2] = ((ts >> 16) & 0x0F) << 4 | (crc4 & 0x0F);

    int16_t temp_fixed = (int16_t)((rec->temperature + 50.0f) * 16.0f);
    raw[3] = (temp_fixed >> 4) & 0xFF;
    raw[4] = ((temp_fixed & 0x0F) << 4) | (rec->flags & 0x0F);

    DebugLn("[Flash] Schreibe Datensatz ins Flash");
    DebugUVal("-> Timestamp", ts, "");
    DebugIVal("-> Temp*16", temp_fixed, "");
    DebugUVal("-> Flags", rec->flags, "");

    bool ok = Flash_PageProgram(FLASH_ADDR_BASE + write_ptr, raw, RECORD_SIZE_BYTES);

    Flash_Close();

    if (ok)
        write_ptr += RECORD_SIZE_BYTES;
    else
        DebugLn("[Flash] Schreiben fehlgeschlagen");

    return ok;
}
/*
bool flash_read_record(uint16_t index, record_t *out) // external flash
{
    if (!out || index >= MAX_RECORDS)
        return FALSE;

    if (!Flash_Open())
    {
        DebugLn("[flash] Open flash failed");
        return FALSE;
    }

    uint8_t raw[RECORD_SIZE_BYTES];
    uint32_t addr = FLASH_ADDR_BASE + index * RECORD_SIZE_BYTES;

    uint16_t addr_lo = (uint16_t)(addr & 0xFFFF);         // Low-Word (untere 16 Bit)
    uint16_t addr_hi = (uint16_t)((addr >> 16) & 0xFFFF); // High-Word (obere 16 Bit)

    DebugUVal("Read Addr LOW ", addr_lo, "");
    DebugUVal("Read Addr HIGH", addr_hi, "");

    //bool ok =
     Flash_ReadData(addr, raw, RECORD_SIZE_BYTES);

    Flash_Close();

  //  if (!ok)
  //  {
   //     DebugLn("[flash] Read fail");
//        // DebugUVal("[flash] Rad fail at index ", index, "");
//        return FALSE;
 //   }

    uint32_t ts = ((uint32_t)(raw[2] >> 4) << 16) | ((uint16_t)raw[1] << 8) | raw[0];
    uint8_t crc4 = raw[2] & 0x0F;
    uint8_t expected_crc = crc4_timestamp(ts);
    if ((expected_crc & 0x0F) != crc4)
    {
        DebugLn("[flash] CRC error");
        // DebugUVal("[flash] CRC4 Fehler bei Index ", index, "");
        return FALSE;
    }

    int16_t temp_fixed = ((int16_t)raw[3] << 4) | (raw[4] >> 4);

    out->timestamp = ts;
    out->temperature = ((float)temp_fixed / 16.0f) - 50.0f;
    out->flags = raw[4] & 0x0F;

    //  DebugUVal("[flash] Gelesen: Timestamp", ts, "");
    //   DebugIVal("          Temp", (int)(out->temperature * 100), " x0.01C");
    ///  DebugUVal("          Flags", out->flags, "");

    return TRUE;
}*/
/*
uint16_t flash_get_count(void) // external flash
{
    return (uint16_t)(write_ptr / RECORD_SIZE_BYTES);
}*/
/*
bool flash_write_record_nolock(const record_t *rec) // external flash
{
    DebugLn("[flash_write_record_nolock]");
    DebugUVal("write_ptr = ", write_ptr, "");
    DebugUVal("BASE ADDR = ", FLASH_ADDR_BASE, "");

    if (!rec)
        return FALSE;

    uint8_t raw[RECORD_SIZE_BYTES];
    uint32_t ts = rec->timestamp;
    uint8_t crc4 = crc4_timestamp(ts);
    raw[0] = (ts >> 0) & 0xFF;
    raw[1] = (ts >> 8) & 0xFF;
    raw[2] = ((ts >> 16) & 0x0F) << 4 | (crc4 & 0x0F);

    int16_t temp_fixed = (int16_t)((rec->temperature + 50.0f) * 16.0f);
    raw[3] = (temp_fixed >> 4) & 0xFF;
    raw[4] = ((temp_fixed & 0x0F) << 4) | (rec->flags & 0x0F);

    DebugLn("[flash] Schreibe Datensatz ins Flash (no-lock)");
    DebugUVal("-> Timestamp   ", ts, "");
    DebugFVal("-> Temperature ", rec->temperature, "degC");
    DebugIVal("-> Temp*16 int ", temp_fixed, "");
    DebugUVal("-> Flags       ", rec->flags, "");

    Flash_Open();
    delay(5);
    Flash_PageProgram(FLASH_ADDR_BASE + write_ptr, raw, RECORD_SIZE_BYTES) &&
        ((write_ptr += RECORD_SIZE_BYTES), TRUE);
    Flash_Close();
    delay(5);

    return TRUE;
}*/
/*
void storage_flash_test(void) // external flash
{
    DebugLn("=== [Flat Flash Test: storage.c] ===");
    Flash_Open();
    Flash_ReadJEDEC_ID();
    Flash_Close();

    const uint32_t test_addr = FLASH_ADDR_BASE;
    const uint16_t test_len = 64;

    uint8_t write_buf[64];
    uint8_t read_buf[64];
    bool success = TRUE;

    // 1. Testdaten erzeugen
    for (uint8_t i = 0; i < test_len; i++)
        write_buf[i] = i ^ 0x5A;

    // 2. Öffnen
    if (!Flash_Open())
    {
        DebugLn("[storage_flat] Flash Open fehlgeschlagen");
        return;
    }

    // 3. Schreiben
    DebugLn("[storage_flat] Schreibe Testdaten...");
    if (!Flash_PageProgram(test_addr, write_buf, test_len))
    {
        DebugLn("[storage_flat] Schreiben fehlgeschlagen");
        Flash_Close();
        return;
    }

    Flash_Close();
    delay(5); // minimaler Flash-Write-Puffer-Zyklus

    // 4. Lesen
    for (uint8_t i = 0; i < test_len; i++)
        read_buf[i] = 0;

    if (!Flash_Open())
    {
        DebugLn("[storage_flat] Flash Re-Open fehlgeschlagen");
        return;
    }


    Flash_ReadData(test_addr, read_buf, test_len);

 //   if (!Flash_ReadData(test_addr, read_buf, test_len))
 //   {
 //       DebugLn("[storage_flat] Lesen fehlgeschlagen");
  //      Flash_Close();
  //      return;
  //  }

    Flash_Close();

    // 5. Validieren
    for (uint8_t i = 0; i < 5; i++) // nur die ersten 5 prüfen
    {
        if (read_buf[i] != write_buf[i])
        {
            success = FALSE;
            UART1_SendString("[FAIL] Byte ");
            UART1_SendHexByte(i);
            UART1_SendString(": soll=");
            UART1_SendHexByte(write_buf[i]);
            UART1_SendString(" ist=");
            UART1_SendHexByte(read_buf[i]);
            UART1_SendString("\r\n");
        }
    }

    if (success)
        DebugLn("[PASS] Flash-Test erfolgreich");
    else
        DebugLn("[FAIL] Flash-Daten stimmen nicht überein");
}*/

/*void storage_debug_dump_records(void)
{
    DebugLn("=== [DEBUG] Flash-Dump: Erste 3 Datensätze ===");

    record_t rec;

    DebugLn("-- Interner Flash --");
    // i = 0
    internal_flash_read_record(0, &rec);
    DebugUVal("Index ", 0, "");
    DebugUVal("  Timestamp (x5min):", rec.timestamp, "");
    DebugFVal("  Temperatur: ", rec.temperature, "°C");
    DebugUVal("  Flags:", rec.flags, ""); DebugLn("");
    // i = 1
    internal_flash_read_record(1, &rec);
    DebugUVal("Index ", 1, "");
    DebugUVal("  Timestamp (x5min):", rec.timestamp, "");
    DebugFVal("  Temperatur: ", rec.temperature, "°C");
    DebugUVal("  Flags:", rec.flags, ""); DebugLn("");
    // i = 2
    internal_flash_read_record(2, &rec);
    DebugUVal("Index ", 2, "");
    DebugUVal("  Timestamp (x5min):", rec.timestamp, "");
    DebugFVal("  Temperatur: ", rec.temperature, "°C");
    DebugUVal("  Flags:", rec.flags, ""); DebugLn("");
*/
/*
DebugLn("-- Externer Flash --");
for (uint8_t i = 0; i < 3; i++)
{
    if (flash_read_record(i, &rec))
    {
        DebugUVal("Index ", i, "");
        DebugUVal("  Timestamp (x5min):", rec.timestamp, "");
        DebugFVal("  Temperatur: ", rec.temperature, "°C");
        DebugUVal("  Flags:", rec.flags, "");
    }
    else
    {
        DebugUVal("[!] Kein gültiger externer Datensatz bei Index ", i, "");
        break;
    }
}*/

/*   DebugLn("=== [ENDE] ===");
}*/

/* bool copy_internal_to_external_flash(void)   // deprecated: storage of records now in RAM
{
    uint16_t total = internal_flash_get_count();
    if (total == 0)
    {
        DebugLn("[copy] Keine internen Datensätze vorhanden, nichts zu kopieren.");
        return TRUE;
    }

    DebugUVal("[copy] Anzahl zu kopierender Datensätze: ", total, "");

    if (!Flash_Open())
    {
        DebugLn("[copy] Flash konnte nicht geöffnet werden.");
        return FALSE;
    }
    else
        DebugLn("[copy] Flash opened successfully");

    for (uint16_t i = 0; i < total; i++)
    {
        record_t rec;
        if (!internal_flash_read_record(i, &rec))
        {
            DebugUVal("[copy] Fehler beim Lesen Index: ", i, "");
            Flash_Close();
            return FALSE;
        }

        if (!flash_write_record_nolock(&rec)) // Neue Funktion ohne open/close
        {
            DebugUVal("[copy] Fehler beim Schreiben ins externe Flash bei Index: ", i, "");
            Flash_Close();
            return FALSE;
        }
    }

    Flash_Close();
    DebugLn("[copy] Alle internen Datensätze erfolgreich ins externe Flash kopiert!");
    return TRUE;
}
    */
