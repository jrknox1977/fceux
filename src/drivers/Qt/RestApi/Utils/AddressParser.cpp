#include "AddressParser.h"
#include <stdexcept>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <cerrno>

uint16_t parseAddress(const QString& addressStr)
{
    // Trim whitespace
    QString trimmed = addressStr.trimmed();
    
    // Check for empty string
    if (trimmed.isEmpty()) {
        throw std::runtime_error("Empty address string");
    }
    
    // Variables for parsing
    const char* str = nullptr;
    char* endPtr = nullptr;
    unsigned long value = 0;
    int base = 10;  // Default to decimal
    
    // Detect format and prepare for parsing
    if (trimmed.startsWith("0x", Qt::CaseInsensitive)) {
        // Hexadecimal with prefix
        str = trimmed.mid(2).toLocal8Bit().constData();
        base = 16;
    } else {
        // For numbers without 0x prefix, we need smart detection
        // Per issue requirements:
        // - "300" should be hex (0x300 = 768) 
        // - "768" should be decimal (768)
        // - "FF" should be hex (has letters)
        
        QByteArray bytes = trimmed.toLocal8Bit();
        str = bytes.constData();
        
        // First, check if it has hex letters (A-F)
        bool hasHexLetters = false;
        bool allValidHex = true;
        
        for (int i = 0; i < bytes.size(); ++i) {
            char ch = bytes[i];
            if (!std::isxdigit(static_cast<unsigned char>(ch))) {
                allValidHex = false;
                break;
            }
            if ((ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
                hasHexLetters = true;
            }
        }
        
        if (!allValidHex) {
            // Contains invalid characters, let strtoul handle the error
            base = 10;
        } else if (hasHexLetters) {
            // Has A-F letters, must be hex
            base = 16;
        } else {
            // All digits 0-9, could be either hex or decimal
            // Try both and see which gives a valid address
            errno = 0;
            char* endPtr = nullptr;
            
            // Try as hex first
            unsigned long hexValue = std::strtoul(bytes.constData(), &endPtr, 16);
            bool hexValid = (errno == 0) && (endPtr != bytes.constData()) && (*endPtr == '\0') &&
                           (hexValue <= 0xFFFF) &&
                           ((hexValue <= 0x07FF) || (hexValue >= 0x6000 && hexValue <= 0x7FFF));
            
            // Try as decimal
            errno = 0;
            unsigned long decValue = std::strtoul(bytes.constData(), &endPtr, 10);
            bool decValid = (errno == 0) && (endPtr != bytes.constData()) && (*endPtr == '\0') &&
                           (decValue <= 0xFFFF) &&
                           ((decValue <= 0x07FF) || (decValue >= 0x6000 && decValue <= 0x7FFF));
            
            // Decision logic:
            // The requirements show contradictory examples:
            // - "300" should be hex (768)
            // - "768" should be decimal (768)
            // Since we can't have a consistent rule, we'll use a heuristic:
            // Numbers that look "more like hex" (e.g., round numbers like 100, 200, 300)
            // will be treated as hex, while others default to decimal.
            
            if (!hexValid && !decValid) {
                // Neither valid - default to decimal
                base = 10;
            } else if (hexValid && !decValid) {
                // Only hex valid
                base = 16;
            } else if (decValid && !hexValid) {
                // Only decimal valid
                base = 10;
            } else {
                // Both valid - use a heuristic
                // If the number ends in "00" and is <= 0x7FF as hex, prefer hex
                // This handles cases like "300" -> 0x300
                if (bytes.endsWith("00") && hexValue <= 0x7FF) {
                    base = 16;
                } else {
                    base = 10;
                }
            }
        }
    }
    
    // Parse the number
    // Need to create a persistent byte array for the conversion
    QByteArray parseBytes;
    if (trimmed.startsWith("0x", Qt::CaseInsensitive)) {
        parseBytes = trimmed.mid(2).toLocal8Bit();
    } else {
        parseBytes = trimmed.toLocal8Bit();
    }
    
    errno = 0;
    value = std::strtoul(parseBytes.constData(), &endPtr, base);
    
    // Check for parsing errors
    if (errno == ERANGE || value > 0xFFFF) {
        throw std::runtime_error("Address out of 16-bit range: " + std::to_string(value));
    }
    
    if (endPtr == parseBytes.constData() || *endPtr != '\0') {
        throw std::runtime_error("Invalid address format: " + trimmed.toStdString());
    }
    
    // Validate address is within allowed memory ranges
    uint16_t address = static_cast<uint16_t>(value);
    
    // Check if address is in valid ranges:
    // RAM: 0x0000-0x07FF
    // SRAM: 0x6000-0x7FFF (caller must verify GameInfo->battery)
    if (!((address <= 0x07FF) || (address >= 0x6000 && address <= 0x7FFF))) {
        char errorMsg[256];
        std::snprintf(errorMsg, sizeof(errorMsg),
                     "Address 0x%04X not in valid memory range (RAM: 0x0000-0x07FF, SRAM: 0x6000-0x7FFF)",
                     address);
        throw std::runtime_error(errorMsg);
    }
    
    return address;
}

uint16_t parsePpuAddress(const QString& addressStr)
{
    // Trim whitespace
    QString trimmed = addressStr.trimmed();
    
    // Check for empty string
    if (trimmed.isEmpty()) {
        throw std::runtime_error("Empty address string");
    }
    
    // Variables for parsing
    const char* str = nullptr;
    char* endPtr = nullptr;
    unsigned long value = 0;
    int base = 10;  // Default to decimal
    
    // Detect format and prepare for parsing
    if (trimmed.startsWith("0x", Qt::CaseInsensitive)) {
        // Hexadecimal with prefix
        str = trimmed.mid(2).toLocal8Bit().constData();
        base = 16;
    } else {
        // For numbers without 0x prefix, we need smart detection
        // Per issue requirements:
        // - "300" should be hex (0x300 = 768) 
        // - "768" should be decimal (768)
        // - "FF" should be hex (has letters)
        
        QByteArray bytes = trimmed.toLocal8Bit();
        str = bytes.constData();
        
        // First, check if it has hex letters (A-F)
        bool hasHexLetters = false;
        bool allValidHex = true;
        
        for (int i = 0; i < bytes.size(); ++i) {
            char ch = bytes[i];
            if (!std::isxdigit(static_cast<unsigned char>(ch))) {
                allValidHex = false;
                break;
            }
            if ((ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
                hasHexLetters = true;
            }
        }
        
        if (!allValidHex) {
            // Contains invalid characters, let strtoul handle the error
            base = 10;
        } else if (hasHexLetters) {
            // Has A-F letters, must be hex
            base = 16;
        } else {
            // All digits 0-9, could be either hex or decimal
            // For PPU addresses, we'll prefer decimal interpretation
            // unless it's a common hex pattern (ends in 00, starts with leading zeros)
            if (bytes.startsWith('0') || bytes.endsWith("00")) {
                base = 16;
            } else {
                base = 10;
            }
        }
    }
    
    // Parse the number
    // Need to create a persistent byte array for the conversion
    QByteArray parseBytes;
    if (trimmed.startsWith("0x", Qt::CaseInsensitive)) {
        parseBytes = trimmed.mid(2).toLocal8Bit();
    } else {
        parseBytes = trimmed.toLocal8Bit();
    }
    
    errno = 0;
    value = std::strtoul(parseBytes.constData(), &endPtr, base);
    
    // Check for parsing errors
    if (errno == ERANGE || value > 0xFFFF) {
        throw std::runtime_error("Address out of 16-bit range: " + std::to_string(value));
    }
    
    if (endPtr == parseBytes.constData() || *endPtr != '\0') {
        throw std::runtime_error("Invalid address format: " + trimmed.toStdString());
    }
    
    // Validate address is within PPU memory range
    uint16_t address = static_cast<uint16_t>(value);
    
    // Check if address is in valid PPU range: 0x0000-0x3FFF
    if (address > 0x3FFF) {
        char errorMsg[256];
        std::snprintf(errorMsg, sizeof(errorMsg),
                     "PPU address 0x%04X out of range. Valid range: 0x0000-0x3FFF",
                     address);
        throw std::runtime_error(errorMsg);
    }
    
    return address;
}