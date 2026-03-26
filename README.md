# TrailCurrent Picket

Sensor module that monitors cabinet and door open/closed states using reed switches and reports status over a CAN bus interface. Part of the [TrailCurrent](https://trailcurrent.com) open-source vehicle platform.

## Hardware Overview

- **Board:** [Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/esp32-s3-rs485-can.htm)
- **Microcontroller:** ESP32-S3
- **Framework:** ESP-IDF
- **Function:** Cabinet and door state monitoring with CAN bus reporting
- **Key Features:**
  - 13 reed switch inputs for open/closed detection
  - CAN bus communication at 500 kbps (onboard TJA1051 transceiver)
  - Firmware-configurable CAN address via NVS (up to 8 modules per bus)
  - Over-the-air (OTA) firmware updates via WiFi (triggered over CAN)
  - Onboard status LED
  - Custom flash partition layout with dual OTA slots

### Design Specifications

- **Operating Temperature:** -20°C to +70°C
- **Power:** 7-36V DC input via onboard buck converter
- **Data Rate:** 5 transmissions per second

## CAN Bus Addressing

The CAN message ID is set at compile time via the `PICKET_ADDRESS` build flag. Valid values are **0 through 7**, giving 8 possible modules on the same bus. The CAN ID is computed as `0x0A + PICKET_ADDRESS`. Default is 0 if not specified. The build will fail if a value outside 0-7 is used.

```bash
# Build with default address 0 (CAN ID 0x0A)
idf.py build

# Build for address 3 (CAN ID 0x0D)
idf.py build -DPICKET_ADDRESS=3
```

| `PICKET_ADDRESS` | CAN ID | DBC Message Name     |
|-------------------|--------|----------------------|
| 0 (default)       | 0x0A   | PicketStatus0        |
| 1                 | 0x0B   | PicketStatus1        |
| 2                 | 0x0C   | PicketStatus2        |
| 3                 | 0x0D   | PicketStatus3        |
| 4                 | 0x0E   | PicketStatus4        |
| 5                 | 0x0F   | PicketStatus5        |
| 6                 | 0x10   | PicketStatus6        |
| 7                 | 0x11   | PicketStatus7        |

### CAN Message Format

Each module transmits a 2-byte message at 5 Hz (200 ms interval):

| Byte | Bits  | Description                          |
|------|-------|--------------------------------------|
| 0    | 0-7   | Door status 1-8 (RSW01-RSW08)       |
| 1    | 0-4   | Door status 9-13 (RSW09-RSW13)      |
| 1    | 5-7   | Reserved                             |

Each bit represents one reed switch: `1` = door open, `0` = door closed.

### CAN Control Messages

The module also listens for control messages from other nodes:

- **CAN ID 0x00 - OTA Update Notification:** Contains a 3-byte MAC address suffix. If it matches this module's hostname, the module connects to WiFi using stored credentials and enters OTA update mode.
- **CAN ID 0x01 - WiFi Credential Configuration:** Multi-message protocol to receive and store WiFi SSID and password in NVS flash for future OTA updates.

## GPIO Pin Assignments

### Onboard (not on pin header)

| GPIO | Function       | Notes                                |
|------|---------------|--------------------------------------|
| 0    | Status LED    | Onboard LED                          |
| 15   | CAN TX        | Onboard TJA1051 transceiver          |
| 16   | CAN RX        | Onboard TJA1051 transceiver          |
| 19   | USB D_N       | USB Serial/JTAG (flash + monitor)    |
| 20   | USB D_P       | USB Serial/JTAG (flash + monitor)    |

### Pin Header (reed switch inputs)

| GPIO | Function | Notes                            |
|------|----------|----------------------------------|
| 4    | RSW01    | Internal pull-up, no ext. resistor |
| 5    | RSW02    | Internal pull-up, no ext. resistor |
| 6    | RSW03    | Internal pull-up, no ext. resistor |
| 7    | RSW04    | Internal pull-up, no ext. resistor |
| 8    | RSW05    | Internal pull-up, no ext. resistor |
| 9    | RSW06    | Internal pull-up, no ext. resistor |
| 10   | RSW07    | Internal pull-up, no ext. resistor |
| 11   | RSW08    | Internal pull-up, no ext. resistor |
| 12   | RSW09    | Internal pull-up, no ext. resistor |
| 13   | RSW10    | Internal pull-up, no ext. resistor |
| 14   | RSW11    | Internal pull-up, no ext. resistor |
| 43   | RSW12    | Internal pull-up, no ext. resistor |
| 44   | RSW13    | Internal pull-up, no ext. resistor |

## Hardware Requirements

### Components

- **Board:** [Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/esp32-s3-rs485-can.htm) — industrial-grade board with onboard CAN transceiver, buck converter (7-36V input), and pin header for sensor connections
- **Sensors:** Normally Open (NO) reed switches
- **Connectors:** Wire to pin header (2x10, 2.54mm pitch)

### KiCAD Library Dependencies

This project uses the consolidated [TrailCurrentKiCADLibraries](https://github.com/trailcurrentoss/TrailCurrentKiCADLibraries).

**Setup:**

```bash
# Clone the library alongside this project
git clone git@github.com:trailcurrentoss/TrailCurrentKiCADLibraries.git

# Set environment variables (add to ~/.bashrc or ~/.zshrc)
# Adjust paths to where you cloned the library
export TRAILCURRENT_SYMBOL_DIR="../TrailCurrentKiCADLibraries/symbols"
export TRAILCURRENT_FOOTPRINT_DIR="../TrailCurrentKiCADLibraries/footprints"
export TRAILCURRENT_3DMODEL_DIR="../TrailCurrentKiCADLibraries/3d_models"
```

See [KICAD_ENVIRONMENT_SETUP.md](https://github.com/trailcurrentoss/TrailCurrentKiCADLibraries/blob/main/KICAD_ENVIRONMENT_SETUP.md) in the library repository for detailed setup instructions.

## Firmware

The firmware is built with ESP-IDF (not PlatformIO/Arduino).

### Build & Flash

```bash
# Source ESP-IDF environment
source ~/esp/v5.5.2/esp-idf/export.sh

# Set target (first time only)
idf.py set-target esp32s3

# Build (default address 0, CAN ID 0x0A)
idf.py build

# Build with a specific module address (0-7)
idf.py build -DPICKET_ADDRESS=3

# Flash via USB
idf.py -p /dev/ttyACM0 flash

# Monitor serial output
idf.py -p /dev/ttyACM0 monitor
```

### OTA Update

```bash
curl -X POST http://esp32-XXYYZZ.local/ota --data-binary @build/picket.bin
```

Where `XXYYZZ` is the last 3 bytes of the device's WiFi MAC address (printed at boot).

## Opening the Project

1. **Set up environment variables** (see Library Dependencies above)
2. **Open KiCAD:**
   ```bash
   kicad EDA/TrailCurrentPicketModule/TrailCurrentPicketModule.kicad_pro
   ```
3. **Verify libraries load** - All symbol and footprint libraries should resolve without errors
4. **View 3D models** - Open PCB and press `Alt+3` to view the 3D visualization

## Manufacturing

- **PCB Files:** Ready for fabrication via standard PCB services (JLCPCB, OSH Park, etc.)
- **BOM Generation:** Export BOM from KiCAD schematic (Tools > Generate BOM)
- **Enclosure:** FreeCAD design included in `CAD/` directory
- **JLCPCB Assembly:** See [BOM_ASSEMBLY_WORKFLOW.md](https://github.com/trailcurrentoss/TrailCurrentKiCADLibraries/blob/main/BOM_ASSEMBLY_WORKFLOW.md) for detailed assembly workflow

## Documentation

- **Requirements:** See `DOCS/Requirements/high-level-requirements.md` for detailed specifications

## Project Structure

```
├── CAD/                          # FreeCAD enclosure design
├── DOCS/                         # Requirements documentation
│   └── Requirements/
│       └── high-level-requirements.md
├── EDA/                          # KiCAD hardware design files
│   └── TrailCurrentPicketModule/
│       ├── *.kicad_pro           # KiCAD project
│       ├── *.kicad_sch           # Schematic
│       └── *.kicad_pcb           # PCB layout
├── main/                         # ESP-IDF firmware source
│   ├── main.c                    # Main application
│   ├── ota.c                     # OTA update implementation
│   ├── ota.h                     # OTA public API
│   ├── CMakeLists.txt            # Component build config
│   └── idf_component.yml        # Component dependencies
├── CMakeLists.txt                # ESP-IDF project root
├── partitions.csv                # Flash partition layout
└── sdkconfig.defaults            # Default build configuration
```

## License

MIT License - See LICENSE file for details.

This is open source hardware. You are free to use, modify, and distribute these designs for personal or commercial purposes.

## Contributing

Improvements and contributions are welcome! Please submit issues or pull requests.

## Support

For questions about:
- **KiCAD setup:** See [KICAD_ENVIRONMENT_SETUP.md](https://github.com/trailcurrentoss/TrailCurrentKiCADLibraries/blob/main/KICAD_ENVIRONMENT_SETUP.md)
- **Assembly workflow:** See [BOM_ASSEMBLY_WORKFLOW.md](https://github.com/trailcurrentoss/TrailCurrentKiCADLibraries/blob/main/BOM_ASSEMBLY_WORKFLOW.md)
