#include "modules/storage.h"
#include "modules/storage_internal.h"
#include "common/types.h"
#include "periphery/flash.h"
#include "utility/debug.h"
#include "stm8s_flash.h"
#include <string.h>

#define EEPROM_ADDR_MODE        0x10
#define EEPROM_ADDR_DEVICE_ID   0x20

#define DEVICE_ID_MAGIC         0xA5
#define DEVICE_ID_LENGTH        4
#define DEVICE_ID_TOTAL_SIZE    (1 + DEVICE_ID_LENGTH)

#define FLASH_ADDR_BASE         0x000100UL
#define RECORD_SIZE_BYTES       5
#define MAX_RECORDS             2048

static uint32_t write_ptr = 0;

void storage_eeprom_unlock(void) {
    DebugLn("[storage] EEPROM entsperrt");
    FLASH_Unlock(FLASH_MEMTYPE_DATA);
}

void storage_write_eeprom(uint16_t address, const uint8_t* data, uint16_t len) {
    DebugVal("[storage] Schreibe EEPROM @", address, "");
    for (uint16_t i = 0; i < len; ++i) {
        FLASH_ProgramByte(FLASH_DATA_START_PHYSICAL_ADDRESS + address + i, data[i]);
    }
}

bool storage_read_eeprom(uint16_t address, uint8_t* data, uint16_t len) {
    for (uint16_t i = 0; i < len; ++i) {
        data[i] = *(uint8_t*)(FLASH_DATA_START_PHYSICAL_ADDRESS + address + i);
    }
    return TRUE;
}

static uint8_t calc_crc8(uint8_t value) {
    uint8_t crc = 0;
    crc ^= value;
    for (uint8_t i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x1D : (crc << 1);
    return crc;
}

static uint8_t crc4_timestamp(uint32_t ts_5min) {
    uint8_t crc = 0;
    crc ^= (ts_5min >> 0) & 0xFF;
    crc ^= (ts_5min >> 8) & 0xFF;
    crc ^= (ts_5min >> 16) & 0x0F;
    for (uint8_t i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? (crc << 1) ^ 0x03 : (crc << 1);
    return crc & 0x0F;
}

void persist_current_mode(mode_t mode) {
    DebugVal("[storage] Speichere Modus:", mode, "");
    uint8_t value = (uint8_t)mode;
    uint8_t crc = calc_crc8(value);
    storage_write_eeprom(EEPROM_ADDR_MODE, &value, 1);
    storage_write_eeprom(EEPROM_ADDR_MODE + 1, &crc, 1);
}

bool load_persisted_mode(mode_t* out_mode) {
    uint8_t value, crc;
    storage_read_eeprom(EEPROM_ADDR_MODE, &value, 1);
    storage_read_eeprom(EEPROM_ADDR_MODE + 1, &crc, 1);
    if (calc_crc8(value) == crc) {
        *out_mode = (mode_t)value;
        DebugVal("[storage] Geladener Modus:", value, "");
        return TRUE;
    }
    DebugLn("[storage] CRC Fehler beim Laden des Modus");
    return FALSE;
}

bool load_device_id(uint8_t* out) {
    uint8_t buf[DEVICE_ID_TOTAL_SIZE];
    if (!storage_read_eeprom(EEPROM_ADDR_DEVICE_ID, buf, DEVICE_ID_TOTAL_SIZE)) {
        DebugLn("[storage] Fehler beim Lesen der Geräte-ID");
        return FALSE;
    }
    if (buf[0] != DEVICE_ID_MAGIC) {
        DebugLn("[storage] Geräte-ID Magic Byte ungültig");
        return FALSE;
    }
    memcpy(out, &buf[1], DEVICE_ID_LENGTH);
    DebugLn("[storage] Geräte-ID geladen");
    return TRUE;
}

bool store_device_id(const uint8_t* id) {
    uint8_t check[1];
    if (!storage_read_eeprom(EEPROM_ADDR_DEVICE_ID, check, 1)) {
        DebugLn("[storage] Geräte-ID Prüfung fehlgeschlagen");
        return FALSE;
    }
    if (check[0] == DEVICE_ID_MAGIC) {
        DebugLn("[storage] Geräte-ID bereits gesetzt – kein Überschreiben erlaubt");
        return FALSE;
    }
    uint8_t buf[DEVICE_ID_TOTAL_SIZE];
    buf[0] = DEVICE_ID_MAGIC;
    memcpy(&buf[1], id, DEVICE_ID_LENGTH);
    storage_write_eeprom(EEPROM_ADDR_DEVICE_ID, buf, DEVICE_ID_TOTAL_SIZE);
    DebugLn("[storage] Neue Geräte-ID gespeichert");
    return TRUE;
}

bool flash_write_record(const record_t* rec) {
    if (!rec) return FALSE;

    uint8_t raw[RECORD_SIZE_BYTES];
    uint32_t ts = rec->timestamp;
    uint8_t crc4 = crc4_timestamp(ts);
    raw[0] = (ts >> 0) & 0xFF;
    raw[1] = (ts >> 8) & 0xFF;
    raw[2] = ((ts >> 16) & 0x0F) << 4 | (crc4 & 0x0F);

    int16_t temp_fixed = (int16_t)((rec->temperature + 50.0f) * 16.0f);
    raw[3] = (temp_fixed >> 4) & 0xFF;
    raw[4] = ((temp_fixed & 0x0F) << 4) | (rec->flags & 0x0F);

    DebugLn("[flash] Schreibe Datensatz ins Flash");
    DebugVal("→ Timestamp", ts, "");
    DebugVal("→ Temp*16", temp_fixed, "");
    DebugVal("→ Flags", rec->flags, "");

    Flash_PageProgram(FLASH_ADDR_BASE + write_ptr, raw, RECORD_SIZE_BYTES);
    write_ptr += RECORD_SIZE_BYTES;

    return TRUE;
}

bool flash_read_record(uint16_t index, record_t* out) {
    if (!out || index >= MAX_RECORDS) return FALSE;

    uint8_t raw[RECORD_SIZE_BYTES];
    uint32_t addr = FLASH_ADDR_BASE + index * RECORD_SIZE_BYTES;
    Flash_ReadData(addr, raw, RECORD_SIZE_BYTES);

    uint32_t ts = ((uint32_t)(raw[2] >> 4) << 16) | ((uint16_t)raw[1] << 8) | raw[0];
    uint8_t crc4 = raw[2] & 0x0F;
    uint8_t expected_crc = crc4_timestamp(ts);
    if ((expected_crc & 0x0F) != crc4) {
        DebugVal("[flash] CRC4 Fehler bei Index ", index, "");
        return FALSE;
    }

    int16_t temp_fixed = ((int16_t)raw[3] << 4) | (raw[4] >> 4);

    out->timestamp = ts;
    out->temperature = ((float)temp_fixed / 16.0f) - 50.0f;
    out->flags = raw[4] & 0x0F;

    DebugVal("[flash] Gelesen: Timestamp", ts, "");
    DebugVal("          Temp", (int)(out->temperature * 100), " x0.01C");
    DebugVal("          Flags", out->flags, "");

    return TRUE;
}

uint16_t flash_get_count(void) {
    return (uint16_t)(write_ptr / RECORD_SIZE_BYTES);
}

bool copy_internal_to_external_flash(void) {
    uint16_t total = internal_flash_get_count();
    if (total == 0) {
        DebugLn("[copy] Keine internen Datensätze vorhanden, nichts zu kopieren.");
        return TRUE;
    }

    DebugVal("[copy] Anzahl zu kopierender Datensätze: ", total, "");

    for (uint16_t i = 0; i < total; i++) {
        record_t rec;
        if (!internal_flash_read_record(i, &rec)) {
            DebugVal("[copy] Fehler beim Lesen Index: ", i, "");
            return FALSE;
        }

        if (!flash_write_record(&rec)) {
            DebugVal("[copy] Fehler beim Schreiben ins externe Flash bei Index: ", i, "");
            return FALSE;
        }
    }

    DebugLn("[copy] Alle internen Datensätze erfolgreich ins externe Flash kopiert!");
    return TRUE;
}
