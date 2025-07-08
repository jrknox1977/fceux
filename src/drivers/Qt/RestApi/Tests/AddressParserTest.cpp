/**
 * Standalone test program for AddressParser utility
 * 
 * Tests parsing of memory addresses from REST API input strings.
 * Covers multiple formats (hex with/without prefix, decimal) and
 * validates against NES memory constraints.
 */

#include "../Utils/AddressParser.h"
#include <iostream>
#include <QString>
#include <QCoreApplication>
#include <vector>
#include <tuple>

struct TestCase {
    QString input;
    uint16_t expected;
    bool shouldFail;
    QString description;
};

void runTest(const TestCase& test) {
    try {
        uint16_t result = parseAddress(test.input);
        if (test.shouldFail) {
            std::cout << "❌ FAIL: \"" << test.input.toStdString() 
                      << "\" - Expected exception but got " << result 
                      << " (" << test.description.toStdString() << ")" << std::endl;
        } else if (result != test.expected) {
            std::cout << "❌ FAIL: \"" << test.input.toStdString() 
                      << "\" - Got " << result << ", expected " << test.expected
                      << " (" << test.description.toStdString() << ")" << std::endl;
        } else {
            std::cout << "✅ PASS: \"" << test.input.toStdString() 
                      << "\" -> " << result 
                      << " (" << test.description.toStdString() << ")" << std::endl;
        }
    } catch (const std::runtime_error& e) {
        if (test.shouldFail) {
            std::cout << "✅ PASS: \"" << test.input.toStdString() 
                      << "\" - Threw as expected: " << e.what() 
                      << " (" << test.description.toStdString() << ")" << std::endl;
        } else {
            std::cout << "❌ FAIL: \"" << test.input.toStdString() 
                      << "\" - Unexpected exception: " << e.what()
                      << " (" << test.description.toStdString() << ")" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    std::cout << "\n=== AddressParser Test Suite ===\n" << std::endl;
    
    // Valid test cases
    std::vector<TestCase> validTests = {
        // Hex with prefix
        {"0x300", 768, false, "hex with prefix"},
        {"0x0300", 768, false, "hex with prefix, leading zeros"},
        {"0xFF", 255, false, "hex with prefix"},
        {"0xff", 255, false, "hex with prefix, lowercase"},
        {"0x7FF", 0x7FF, false, "max RAM address"},
        {"0x6000", 0x6000, false, "min SRAM address"},
        {"0x7FFF", 0x7FFF, false, "max SRAM address"},
        
        // Hex without prefix
        {"FF", 255, false, "hex without prefix (has letters)"},
        {"ff", 255, false, "hex without prefix, lowercase"},
        {"300", 768, false, "hex without prefix (heuristic: ends with 00)"},
        
        // Decimal
        {"768", 768, false, "decimal"},
        {"255", 255, false, "decimal"},
        {"2047", 2047, false, "max RAM in decimal"},
        
        // Whitespace
        {"  0x300  ", 768, false, "with whitespace"},
        {"\t768\n", 768, false, "with tabs and newlines"},
    };
    
    // Invalid test cases
    std::vector<TestCase> invalidTests = {
        // Out of range
        {"0x10000", 0, true, "out of 16-bit range"},
        {"65536", 0, true, "out of 16-bit range (decimal)"},
        {"-1", 0, true, "negative number"},
        
        // Invalid memory regions
        {"0x800", 0, true, "between RAM and SRAM"},
        {"0x900", 0, true, "invalid memory region"},
        {"0x5FFF", 0, true, "just before SRAM"},
        {"0x8000", 0, true, "just after SRAM"},
        {"0xFFFF", 0, true, "max 16-bit but invalid region"},
        
        // Invalid formats
        {"", 0, true, "empty string"},
        {"   ", 0, true, "only whitespace"},
        {"invalid", 0, true, "non-numeric"},
        {"12G4", 0, true, "invalid hex character"},
        {"0x", 0, true, "prefix only"},
    };
    
    std::cout << "Running valid input tests:" << std::endl;
    for (const auto& test : validTests) {
        runTest(test);
    }
    
    std::cout << "\nRunning invalid input tests:" << std::endl;
    for (const auto& test : invalidTests) {
        runTest(test);
    }
    
    std::cout << "\n=== Test Summary ===\n" << std::endl;
    std::cout << "Total tests: " << (validTests.size() + invalidTests.size()) << std::endl;
    std::cout << "\nIf all tests show ✅, the implementation is correct!" << std::endl;
    
    return 0;
}