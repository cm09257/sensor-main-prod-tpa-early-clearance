# Sensor Node Firmware (`sensor-main`)

This is the main application firmware for an ultra-low-power embedded temperature sensor node, designed for long-term deployment in asphalt and similar environments. The system is based on an STM8AF5288 microcontroller and integrates multiple SPI/I²C peripherals via a modular sensor library (`sensor-lib`).

## Author

Cedrik Meier  
TronicSoft GmbH  
cedrik.meier@tronicsoft.gmbh

## Architecture Overview

The firmware implements a state-machine architecture with multiple operational modes:

- **MODE_TEST**: For hardware and peripheral validation
- **MODE_WAIT_FOR_ACTIVATION**: Passive state before deployment
- **MODE_OPERATIONAL**: Periodic measurement and flash storage
- **MODE_DATA_TRANSFER**: Buffered data transmission via radio
- **MODE_PRE_HIGH_TEMPERATURE / HIGH_TEMPERATURE**: Threshold-based temperature response
- **MODE_SLEEP / HALT**: Energy-saving mode

## Connected Peripherals

- **TMP126** via SPI – precise temperature measurement
- **MCP7940N** via I²C – real-time clock with dual alarm support
- **AT25-series SPI Flash** – cyclic data logging (temperature, timestamp, flags)
- **RFM69HW** via SPI – ISM-band wireless communication (ACK/NACK + Downlink)

## Build and Usage

This project uses [PlatformIO](https://platformio.org/) with a custom board definition:

```ini
[env:stm8af5288_custom]
platform = ststm8
board = stm8af5288_custom
board_dir = boards
framework = ststm8spl
src_dir = src
include_dir = include
build_flags =
    -Ilib/sensor-lib/include
```

The hardware setup assumes a real-time wake-up via MCP7940N (ALM0/ALM1) and optional ALERT pin interrupt from TMP126.

## Data Format

Data records written to flash follow this structure:

| Field      | Type     | Description                      |
|------------|----------|----------------------------------|
| Timestamp  | `uint32` | Relative time in 5-minute steps  |
| Temperature| `float`  | In °C                            |
| Flags      | `uint8`  | Status + event bitmask           |

## Dependencies

- `sensor-lib` (added as Git submodule)
- STM8 SPL via PlatformIO
- ST-Link for upload/debugging

## Notes

- Code is modularized under `src/` and `include/` with state-specific logic in `modes/`
- Power consumption is optimized through HALT mode and external wakeup sources
- EEPROM configuration with CRC validation is supported

## License

Proprietary.
