#ifndef __ADDRESS_PARSER_H__
#define __ADDRESS_PARSER_H__

#include <QString>
#include <cstdint>

/**
 * @brief Parse a memory address string from REST API input
 * 
 * Converts string representations of addresses into numeric values for the emulator.
 * Supports multiple input formats and validates against NES memory constraints.
 * 
 * Supported formats:
 * - Hexadecimal with prefix: "0x300", "0x0300", "0xFF"
 * - Hexadecimal without prefix: "300", "0300", "FF"
 * - Decimal: "768", "255"
 * - Case insensitive for hex digits
 * 
 * Valid memory ranges:
 * - RAM: 0x0000-0x07FF (2KB main RAM)
 * - SRAM: 0x6000-0x7FFF (battery-backed RAM)
 * 
 * NOTE: This function validates that addresses fall within valid ranges,
 * but SRAM validation (0x6000-0x7FFF) requires the caller to verify
 * that GameInfo->battery is true before attempting SRAM access.
 * 
 * @param addressStr The address string to parse
 * @return uint16_t The parsed address value
 * @throws std::runtime_error if parsing fails or address is invalid
 * 
 * Example usage:
 * @code
 * try {
 *     uint16_t addr = parseAddress("0x300");
 *     // addr == 768
 * } catch (const std::runtime_error& e) {
 *     // Handle error
 * }
 * @endcode
 */
uint16_t parseAddress(const QString& addressStr);

#endif // __ADDRESS_PARSER_H__