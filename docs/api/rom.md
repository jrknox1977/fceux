# ROM Information Endpoint

ROM information endpoint provides metadata about the currently loaded NES ROM file.

## GET /api/rom/info

**Description**: Get detailed information about the currently loaded ROM

**Parameters**: None

**Request Example**:
```bash
curl -X GET http://localhost:8080/api/rom/info
```

**Response** (ROM loaded):
```json
{
  "loaded": true,
  "filename": "Super Mario Bros.nes",
  "name": "SUPER MARIO BROS.",
  "size": 40976,
  "mapper": 0,
  "mirroring": "horizontal",
  "has_battery": false,
  "md5": "811b027eaf99c2def7b933c5208636de"
}
```

**Response** (No ROM loaded):
```json
{
  "loaded": false
}
```

**Response Fields**:

### Always Present
- `loaded`: true if a ROM is currently loaded, false otherwise

### Present when ROM loaded
- `filename`: Original ROM filename (including extension)
- `name`: Internal ROM name (from iNES header, typically uppercase)
- `size`: Total ROM size in bytes (PRG + CHR data)
- `mapper`: NES mapper number (0-255)
- `mirroring`: Screen mirroring type
- `has_battery`: true if cartridge has battery-backed save RAM
- `md5`: MD5 hash of ROM data (32-character hex string)

**Mirroring Types**:
- `"horizontal"`: Horizontal mirroring (Mario Bros, etc.)
- `"vertical"`: Vertical mirroring (Metroid, etc.)
- `"4screen"`: Four-screen mirroring (Gauntlet, etc.)
- `"none"`: No mirroring
- `"unknown"`: Unable to determine mirroring type

**Status Codes**:
- `200 OK`: Information retrieved successfully
- `500 Internal Server Error`: Command execution failed

**Error Response**:
```json
{
  "success": false,
  "error": "Command execution timeout"
}
```

## ROM Size Calculation

The `size` field represents total ROM data:
- **PRG ROM**: Program code and data
- **CHR ROM**: Character/graphics data
- **Total**: PRG size + CHR size in bytes

Common ROM sizes:
- 24KB - 40KB: Simple games (Mario Bros, Pac-Man)
- 128KB - 256KB: Advanced games (Metroid, Zelda)
- 512KB+: Late-generation games with complex mappers

## Mapper Information

The mapper number indicates the memory management controller (MMC) chip:
- **0 (NROM)**: Simplest mapper, 32KB max (Mario Bros, Donkey Kong)
- **1 (MMC1)**: Popular mapper, up to 256KB (Metroid, Zelda)
- **2 (UxROM)**: Simple bank switching (Mega Man, Castlevania)
- **3 (CNROM)**: CHR-ROM switching (Q*bert, Arkanoid)
- **4 (MMC3)**: Advanced mapper with IRQ (Mario 3, Kirby)

## Battery-Backed Saves

Games with `has_battery: true` can save progress:
- Save data stored in 8KB SRAM (0x6000-0x7FFF)
- Powered by cartridge battery when system off
- Examples: Zelda, Final Fantasy, Metroid

## MD5 Hash Usage

The MD5 hash uniquely identifies ROM dumps:
- Used for ROM verification and database lookup
- Helps identify ROM hacks and different regional versions
- 32-character lowercase hexadecimal string
- Calculated from raw ROM data (PRG + CHR)

## Usage Examples

### Basic ROM Check
```bash
#!/bin/bash

# Check if ROM is loaded
LOADED=$(curl -s http://localhost:8080/api/rom/info | jq -r '.loaded')

if [ "$LOADED" = "true" ]; then
    echo "ROM is loaded"
    # Get ROM name
    NAME=$(curl -s http://localhost:8080/api/rom/info | jq -r '.name')
    echo "Playing: $NAME"
else
    echo "No ROM loaded"
fi
```

### ROM Identification
```bash
#!/bin/bash

# Get full ROM info
ROM_INFO=$(curl -s http://localhost:8080/api/rom/info)
LOADED=$(echo $ROM_INFO | jq -r '.loaded')

if [ "$LOADED" = "true" ]; then
    NAME=$(echo $ROM_INFO | jq -r '.name')
    MAPPER=$(echo $ROM_INFO | jq -r '.mapper')
    SIZE=$(echo $ROM_INFO | jq -r '.size')
    MD5=$(echo $ROM_INFO | jq -r '.md5')
    
    echo "ROM: $NAME"
    echo "Mapper: $MAPPER"
    echo "Size: ${SIZE} bytes"
    echo "MD5: $MD5"
else
    echo "No ROM loaded"
fi
```

### Mapper-Specific Logic
```bash
#!/bin/bash

# Check mapper for compatibility
MAPPER=$(curl -s http://localhost:8080/api/rom/info | jq -r '.mapper')

case $MAPPER in
    0)
        echo "NROM mapper - Basic game"
        ;;
    1)
        echo "MMC1 mapper - May have save data"
        ;;
    4)
        echo "MMC3 mapper - Advanced features available"
        ;;
    *)
        echo "Mapper $MAPPER - Check compatibility"
        ;;
esac
```

### Save Game Detection
```bash
#!/bin/bash

# Check if game supports saving
HAS_BATTERY=$(curl -s http://localhost:8080/api/rom/info | jq -r '.has_battery')

if [ "$HAS_BATTERY" = "true" ]; then
    echo "Game supports save data"
    echo "Save RAM available at 0x6000-0x7FFF"
else
    echo "Game uses password/code saves only"
fi
```

### ROM Database Lookup
```bash
#!/bin/bash

# Get MD5 for database lookup
MD5=$(curl -s http://localhost:8080/api/rom/info | jq -r '.md5')

if [ "$MD5" != "null" ]; then
    echo "Looking up ROM: $MD5"
    # Example database lookup (would use real ROM database)
    # curl "https://romdb.example.com/lookup/$MD5"
else
    echo "No ROM loaded for lookup"
fi
```

## File Format Support

FCEUX supports various ROM file formats:
- **.nes**: Standard iNES format (most common)
- **.unif**: UNIF format for complex mappers
- **.fds**: Famicom Disk System images
- **Compressed**: ZIP, GZ, BZ2 archives

The API returns information for all supported formats.

## Regional Differences

Different regional ROM versions may have:
- Different MD5 hashes
- Slight name variations
- Same mapper and size
- Different timing (NTSC/PAL)

## Performance Notes

- ROM info retrieval is fast (< 1ms typical)
- Information cached while ROM loaded
- No emulation state dependencies
- Safe to call during emulation

## Error Handling

Errors are rare for this endpoint:
- No special ROM loading requirements
- Works immediately after ROM load
- Gracefully handles missing ROM data
- 2-second timeout for command execution