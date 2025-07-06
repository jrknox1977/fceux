#!/bin/bash
# FCEUX Ubuntu Setup Script
# Automatically installs all dependencies and builds FCEUX with REST API support

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Functions
print_status() {
    echo -e "${GREEN}[$(date +%H:%M:%S)]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

check_ubuntu_version() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        if [[ "$ID" != "ubuntu" ]]; then
            print_error "This script is designed for Ubuntu. Detected: $ID"
            exit 1
        fi
        
        # Check version (20.04 or newer)
        VERSION_ID_MAJOR=$(echo $VERSION_ID | cut -d. -f1)
        if [ "$VERSION_ID_MAJOR" -lt 20 ]; then
            print_warning "Ubuntu $VERSION_ID detected. Ubuntu 20.04+ recommended."
        else
            print_status "Ubuntu $VERSION_ID detected ✓"
        fi
    else
        print_error "Cannot detect OS version"
        exit 1
    fi
}

install_dependencies() {
    print_status "Installing build dependencies..."
    
    sudo apt update
    
    # Core build tools
    sudo apt install -y \
        build-essential \
        cmake \
        git \
        pkg-config
    
    # Qt5 development libraries
    print_status "Installing Qt5 libraries..."
    sudo apt install -y \
        qtbase5-dev \
        qt5-qmake \
        libqt5opengl5-dev \
        libqt5svg5-dev \
        qttools5-dev \
        qttools5-dev-tools || {
            print_warning "Qt5 installation failed, trying Qt6..."
            QT6_FALLBACK=1
        }
    
    # If Qt5 failed, try Qt6
    if [ "$QT6_FALLBACK" = "1" ]; then
        sudo apt install -y \
            qt6-base-dev \
            qt6-base-dev-tools \
            libqt6opengl6-dev \
            libqt6svg6-dev \
            qt6-tools-dev \
            qt6-tools-dev-tools
        USE_QT6="-DQT6=ON"
    fi
    
    # FCEUX dependencies
    print_status "Installing FCEUX dependencies..."
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
    
    # Optional but recommended
    print_status "Installing optional dependencies..."
    sudo apt install -y \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswresample-dev \
        libswscale-dev \
        libarchive-dev \
        libpng-dev \
        libjpeg-dev \
        libssl-dev || print_warning "Some optional dependencies failed to install"
    
    # Development tools
    print_status "Installing development tools..."
    sudo apt install -y \
        gdb \
        valgrind \
        cppcheck \
        clang-format || print_warning "Some development tools failed to install"
}

setup_rest_api_libs() {
    print_status "Setting up REST API libraries..."
    
    # Create lib directory
    mkdir -p src/lib
    cd src/lib
    
    # Download cpp-httplib
    if [ ! -f "httplib.h" ]; then
        print_status "Downloading cpp-httplib..."
        wget -q https://github.com/yhirose/cpp-httplib/releases/latest/download/httplib.h || {
            print_error "Failed to download cpp-httplib"
            return 1
        }
    fi
    
    # Download nlohmann/json
    if [ ! -f "json.hpp" ]; then
        print_status "Downloading nlohmann/json..."
        wget -q https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp || {
            print_error "Failed to download json library"
            return 1
        }
    fi
    
    cd ../..
    print_status "REST API libraries setup complete ✓"
}

clone_repository() {
    print_status "Cloning FCEUX repository..."
    
    if [ -d "fceux" ]; then
        print_warning "fceux directory already exists"
        read -p "Remove existing directory and re-clone? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf fceux
        else
            cd fceux
            git pull
            return 0
        fi
    fi
    
    git clone https://github.com/jrknox1977/fceux.git
    cd fceux
}

build_fceux() {
    print_status "Building FCEUX..."
    
    # Clean any previous build
    rm -rf build
    mkdir build
    cd build
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DREST_API=ON \
        $USE_QT6 || {
            print_error "CMake configuration failed"
            exit 1
        }
    
    # Build
    print_status "Compiling with $(nproc) cores..."
    make -j$(nproc) || {
        print_error "Build failed"
        exit 1
    }
    
    # Verify build
    if [ -f "./src/fceux" ]; then
        print_status "Build successful! ✓"
        ./src/fceux --help > /dev/null 2>&1 && print_status "Binary test passed ✓"
    else
        print_error "Build verification failed"
        exit 1
    fi
    
    cd ..
}

create_shortcuts() {
    print_status "Creating convenience scripts..."
    
    # Create run script
    cat > run-fceux.sh << 'EOF'
#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
"$SCRIPT_DIR/build/src/fceux" "$@"
EOF
    chmod +x run-fceux.sh
    
    # Create rebuild script
    cat > rebuild.sh << 'EOF'
#!/bin/bash
cd build
make -j$(nproc)
cd ..
EOF
    chmod +x rebuild.sh
    
    print_status "Created run-fceux.sh and rebuild.sh"
}

install_system_wide() {
    read -p "Install FCEUX system-wide? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_status "Installing FCEUX system-wide..."
        cd build
        sudo make install
        cd ..
        print_status "System-wide installation complete ✓"
        print_status "You can now run 'fceux' from anywhere"
    fi
}

print_summary() {
    echo
    echo "========================================"
    echo -e "${GREEN}FCEUX Setup Complete!${NC}"
    echo "========================================"
    echo
    echo "FCEUX has been built successfully with REST API support."
    echo
    echo "To run FCEUX:"
    echo "  ./run-fceux.sh"
    echo "  OR"
    echo "  ./build/src/fceux"
    echo
    if command -v fceux &> /dev/null; then
        echo "System-wide command:"
        echo "  fceux"
        echo
    fi
    echo "To rebuild after changes:"
    echo "  ./rebuild.sh"
    echo
    echo "REST API endpoints will be available at:"
    echo "  http://localhost:8080/api/"
    echo
    echo "For more information:"
    echo "  - Build options: docs/BUILD_QUICKSTART.md"
    echo "  - REST API: docs/FCEUX_REST_API_RESEARCH_REPORT.md"
    echo "  - Development: CLAUDE.md"
    echo
}

# Main execution
main() {
    echo "========================================"
    echo "FCEUX Ubuntu Setup Script"
    echo "========================================"
    echo
    
    # Check Ubuntu version
    check_ubuntu_version
    
    # Install dependencies
    install_dependencies
    
    # Get to the right directory
    if [ ! -d "$HOME/repos" ]; then
        mkdir -p "$HOME/repos"
    fi
    cd "$HOME/repos"
    
    # Clone repository
    clone_repository
    
    # Setup REST API libraries
    setup_rest_api_libs
    
    # Build FCEUX
    build_fceux
    
    # Create convenience scripts
    create_shortcuts
    
    # Optional system-wide install
    install_system_wide
    
    # Print summary
    print_summary
}

# Run main function
main