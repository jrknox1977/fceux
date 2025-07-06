# FCEUX Build Quick Start Guide

This guide provides the fastest path to building FCEUX with REST API support on Ubuntu.

## One-Command Setup (Ubuntu 20.04+)

```bash
# Run this single command to install everything
curl -sSL https://raw.githubusercontent.com/jrknox1977/fceux/master/scripts/ubuntu-setup.sh | bash
```

## Manual Setup Steps

### 1. Install All Dependencies (5 minutes)

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install everything in one command
sudo apt install -y \
    build-essential cmake git \
    qtbase5-dev qt5-qmake libqt5opengl5-dev libqt5svg5-dev qttools5-dev qttools5-dev-tools \
    libsdl2-dev libminizip-dev libgl1-mesa-dev liblua5.1-0-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev \
    libarchive-dev libx264-dev libx265-dev \
    libpng-dev libjpeg-dev libssl-dev \
    gdb valgrind cppcheck clang-format

# For REST API (optional)
mkdir -p ~/repos/fceux/src/lib && cd ~/repos/fceux/src/lib
wget https://github.com/yhirose/cpp-httplib/releases/latest/download/httplib.h
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
cd ~/repos
```

### 2. Clone and Build (3 minutes)

```bash
# Clone repository
git clone https://github.com/jrknox1977/fceux.git
cd fceux

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DREST_API=ON
make -j$(nproc)

# Test
./src/fceux
```

### 3. Install System-Wide (Optional)

```bash
sudo make install
```

## Troubleshooting

### Qt Version Issues
```bash
# Check Qt version
qmake --version

# Use Qt6 if Qt5 not available
cmake .. -DQT6=ON
```

### Missing Library Errors
```bash
# Find what's missing
ldd ./src/fceux | grep "not found"

# Install missing library (replace XXX with library name)
sudo apt search libXXX
sudo apt install libXXX-dev
```

### Build Errors
```bash
# Clean and rebuild
cd .. && rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make VERBOSE=1
```

## Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `-DCMAKE_BUILD_TYPE=Release` | Optimized build | Debug |
| `-DREST_API=ON` | Enable REST API | OFF |
| `-DQT6=ON` | Use Qt6 instead of Qt5 | OFF |
| `-DASAN=ON` | Enable AddressSanitizer | OFF |
| `-DCMAKE_INSTALL_PREFIX=/usr` | Install location | /usr/local |

## Quick Test

```bash
# Run FCEUX
./src/fceux

# Test REST API (if enabled)
curl http://localhost:8080/api/system/info
```

## Next Steps

1. Load a ROM: `./src/fceux rom.nes`
2. Configure: Edit `~/.config/fceux/fceux.cfg`
3. REST API: See `/docs/FCEUX_REST_API_RESEARCH_REPORT.md`
4. Development: See `/CLAUDE.md` for coding guidelines

## Complete Build Script

Save as `build-fceux.sh`:

```bash
#!/bin/bash
set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Building FCEUX with REST API support...${NC}"

# Check if in fceux directory
if [ ! -f "README" ] || [ ! -d "src" ]; then
    echo -e "${RED}Error: Not in FCEUX root directory${NC}"
    exit 1
fi

# Clean previous build
rm -rf build

# Create and enter build directory
mkdir build && cd build

# Configure
echo -e "${GREEN}Configuring...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DREST_API=ON \
    -DCMAKE_INSTALL_PREFIX=/usr

# Build
echo -e "${GREEN}Building with $(nproc) cores...${NC}"
make -j$(nproc)

# Check success
if [ -f ./src/fceux ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}Run with: ./src/fceux${NC}"
    
    # Quick test
    ./src/fceux --help > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Binary test passed!${NC}"
    fi
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
```

## Estimated Time

- Dependency installation: 5-10 minutes
- Clone repository: 1 minute  
- Build FCEUX: 2-5 minutes
- **Total: ~15 minutes**

## System Requirements

- Ubuntu 20.04 LTS or newer
- 4GB RAM minimum
- 2GB free disk space
- Internet connection for downloads