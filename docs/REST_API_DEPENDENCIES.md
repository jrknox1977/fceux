# Additional Dependencies for REST API Implementation

## Required Libraries

### 1. cpp-httplib (Header-only HTTP library)
```bash
# Download the header file
cd ~/repos/fceux
mkdir -p src/lib
cd src/lib
wget https://github.com/yhirose/cpp-httplib/releases/latest/download/httplib.h
```

### 2. JSON Library (nlohmann/json - Header-only)
```bash
# Option 1: Install from Ubuntu repository
sudo apt install nlohmann-json3-dev

# Option 2: Download header directly
cd ~/repos/fceux/src/lib
wget https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
```

### 3. WebSocket Support (Optional - for video streaming)
```bash
# Install WebSocket++ (header-only)
sudo apt install libwebsocketpp-dev

# Or download manually
cd ~/repos/fceux/src/lib
git clone https://github.com/zaphoyd/websocketpp.git
```

### 4. Additional Compression Libraries (for screenshots)
```bash
# For PNG compression (already installed with Qt dependencies)
sudo apt install libpng-dev

# For JPEG support (optional)
sudo apt install libjpeg-dev

# These are likely already installed as Qt dependencies
```

### 5. OpenSSL (for HTTPS support - optional)
```bash
sudo apt install libssl-dev
```

## Build Configuration

Add to CMakeLists.txt:

```cmake
# Enable C++11 for REST API
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# REST API option
option(REST_API "Enable REST API server" ON)

if(REST_API)
    add_definitions(-D__FCEU_REST_API_ENABLE__)
    
    # Include paths for header-only libraries
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
    
    # Find system libraries
    find_package(PNG REQUIRED)
    find_package(JPEG)
    find_package(OpenSSL)
    
    # Optional: Find nlohmann/json if installed system-wide
    find_package(nlohmann_json 3.2.0)
    
    # Link libraries
    if(PNG_FOUND)
        target_link_libraries(fceux ${PNG_LIBRARIES})
    endif()
    
    if(JPEG_FOUND)
        target_link_libraries(fceux ${JPEG_LIBRARIES})
        add_definitions(-DHAVE_LIBJPEG)
    endif()
    
    if(OPENSSL_FOUND)
        target_link_libraries(fceux ${OPENSSL_LIBRARIES})
        add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
    endif()
endif()
```

## Quick Install Script

Save as `install-rest-api-deps.sh`:

```bash
#!/bin/bash
set -e

echo "Installing REST API dependencies..."

# Create lib directory
mkdir -p src/lib
cd src/lib

# Download cpp-httplib
echo "Downloading cpp-httplib..."
wget -q https://github.com/yhirose/cpp-httplib/releases/latest/download/httplib.h

# Download JSON library
echo "Downloading nlohmann/json..."
wget -q https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp

# Install system packages
echo "Installing system packages..."
sudo apt update
sudo apt install -y \
    libpng-dev \
    libjpeg-dev \
    libssl-dev \
    libwebsocketpp-dev

echo "REST API dependencies installed successfully!"

cd ../..
```

Make executable and run:
```bash
chmod +x install-rest-api-deps.sh
./install-rest-api-deps.sh
```

## Verification

After installation, verify headers are available:

```bash
ls -la src/lib/
# Should show:
# httplib.h
# json.hpp

# Verify system libraries
pkg-config --modversion libpng
pkg-config --modversion openssl
```

## Notes

1. **cpp-httplib** is a single-header library that provides both HTTP server and client functionality
2. **nlohmann/json** is the de facto standard for JSON in modern C++
3. **WebSocket++** is only needed if implementing video streaming
4. Most image libraries are already available through Qt dependencies
5. OpenSSL is optional but recommended for production deployments