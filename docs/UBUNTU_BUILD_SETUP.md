# Ubuntu Build Environment Setup for FCEUX

This guide will help you set up a complete build environment for FCEUX on Ubuntu Desktop.

## System Requirements
- Ubuntu 20.04 LTS or newer (22.04 LTS recommended)
- At least 4GB RAM
- ~2GB free disk space

## Step 1: Update System
```bash
sudo apt update
sudo apt upgrade -y
```

## Step 2: Install Build Essentials
```bash
sudo apt install -y build-essential cmake git
```

## Step 3: Install Qt Development Libraries

### For Qt5 (Default):
```bash
sudo apt install -y \
    qtbase5-dev \
    qt5-qmake \
    libqt5opengl5-dev \
    libqt5svg5-dev \
    qttools5-dev \
    qttools5-dev-tools
```

### For Qt6 (Alternative):
```bash
sudo apt install -y \
    qt6-base-dev \
    qt6-base-dev-tools \
    libqt6opengl6-dev \
    libqt6svg6-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools
```

## Step 4: Install Required Dependencies
```bash
sudo apt install -y \
    libsdl2-dev \
    libsdl2-2.0-0 \
    libminizip-dev \
    libglvnd-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    zlib1g-dev \
    liblua5.1-0-dev \
    libx264-dev \
    libx265-dev
```

## Step 5: Install Optional Dependencies (Recommended)
```bash
# For video recording support
sudo apt install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libswscale-dev

# For 7zip archive support
sudo apt install -y libarchive-dev

# For debugging and development
sudo apt install -y \
    gdb \
    valgrind \
    cppcheck \
    clang-format
```

## Step 6: Clone and Build FCEUX

### Clone the Repository
```bash
cd ~
mkdir -p repos
cd repos
git clone https://github.com/jrknox1977/fceux.git
cd fceux
```

### Build FCEUX
```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

# Build with all CPU cores
make -j$(nproc)

# Test the build
./src/fceux
```

### Install to System (Optional)
```bash
sudo make install
```

## Step 7: Verify Installation

### Check all dependencies are satisfied:
```bash
ldd ./src/fceux | grep "not found"
# Should return nothing if all dependencies are met
```

### Run FCEUX:
```bash
./src/fceux
# Or if installed system-wide:
fceux
```

## Troubleshooting

### Qt Version Issues
If you encounter Qt-related errors, check which version is installed:
```bash
qmake --version  # For Qt5
qmake6 --version # For Qt6
```

### Missing Libraries
If you get "cannot find -lXXX" errors during linking:
```bash
# Search for the library
apt search libXXX
# Install the -dev package
sudo apt install libXXX-dev
```

### OpenGL Issues
If you have OpenGL-related errors:
```bash
# Check OpenGL support
glxinfo | grep "OpenGL version"

# Install Mesa drivers if needed
sudo apt install mesa-utils
```

### Debug Build Issues
For development with AddressSanitizer:
```bash
# Clean previous build
rm -rf build/*
cd build

# Build with debug symbols and ASAN
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASAN=ON
make -j$(nproc)
```

## Development Tools Setup

### VS Code (Optional)
```bash
# Install VS Code
sudo snap install --classic code

# Install C++ extensions
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools
```

### Git Configuration
```bash
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

## Next Steps

1. **Test the Build**: Load a ROM file to ensure everything works
2. **Run Static Analysis**: `./scripts/runCppCheck.sh`
3. **Review Documentation**: 
   - `docs/architecture/` - REST API research
   - `CLAUDE.md` - Development guidelines
   - `README` - General information

## Quick Build Script

Save this as `build-fceux.sh`:
```bash
#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Building FCEUX...${NC}"

# Create build directory
mkdir -p build
cd build

# Configure
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

# Build
echo -e "${GREEN}Compiling with $(nproc) cores...${NC}"
make -j$(nproc)

# Check if build succeeded
if [ -f ./src/fceux ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}Run with: ./src/fceux${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
```

Make it executable:
```bash
chmod +x build-fceux.sh
./build-fceux.sh
```