/**
 * @file MemoryReadCommandTest.cpp
 * @brief Unit tests for MemoryReadCommand class
 * 
 * Compile with:
 * g++ -std=c++11 -I../../../../.. -I../../.. \
 *     MemoryReadCommandTest.cpp \
 *     ../Commands/MemoryReadCommand.cpp \
 *     -o MemoryReadCommandTest \
 *     $(pkg-config --cflags --libs Qt5Core) \
 *     -DTEST_MODE
 */

#include <iostream>
#include <cassert>
#include <future>
#include <sstream>
#include "../Commands/MemoryReadCommand.h"

// Mock implementations for testing
#ifdef TEST_MODE

// Mock global variables
void* GameInfo = nullptr;

// Mock FCEU_CheatGetByte function
uint8_t mockMemoryValue = 0x42;
extern "C" uint8_t FCEU_CheatGetByte(uint32_t address) {
    return mockMemoryValue;
}

// Mock mutex functions
void fceuWrapperLock(const char* file, int line, const char* func) {
    // No-op for testing
}

void fceuWrapperUnLock() {
    // No-op for testing
}

#endif // TEST_MODE

// Test helper functions
void testJsonFormat() {
    std::cout << "Testing JSON format..." << std::endl;
    
    MemoryReadResult result;
    result.address = 0x0300;
    result.value = 0x42;
    
    std::string json = result.toJson();
    std::string expected = R"({"address":"0x0300","value":"0x42","decimal":66,"binary":"01000010"})";
    
    assert(json == expected);
    std::cout << "  ✓ JSON format correct: " << json << std::endl;
}

void testSuccessfulRead() {
    std::cout << "Testing successful memory read..." << std::endl;
    
    // Set up mock environment
    int mockGameInfo = 1;
    GameInfo = &mockGameInfo;
    mockMemoryValue = 0x55;
    
    // Create and execute command
    MemoryReadCommand cmd(0x0400);
    auto future = cmd.getResult();
    
    try {
        cmd.execute();
        auto result = future.get();
        
        assert(result.address == 0x0400);
        assert(result.value == 0x55);
        std::cout << "  ✓ Memory read successful" << std::endl;
        std::cout << "  ✓ Result: " << result.toJson() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Unexpected exception: " << e.what() << std::endl;
        assert(false);
    }
}

void testNoGameLoaded() {
    std::cout << "Testing with no game loaded..." << std::endl;
    
    // Set GameInfo to null
    GameInfo = nullptr;
    
    // Create and execute command
    MemoryReadCommand cmd(0x0300);
    auto future = cmd.getResult();
    
    try {
        cmd.execute();
        std::cerr << "  ✗ Expected exception not thrown" << std::endl;
        assert(false);
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg == "No game loaded");
        std::cout << "  ✓ Correct exception thrown: " << msg << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Wrong exception type: " << e.what() << std::endl;
        assert(false);
    }
}

void testVariousAddresses() {
    std::cout << "Testing various address values..." << std::endl;
    
    int mockGameInfo = 1;
    GameInfo = &mockGameInfo;
    
    // Test addresses
    uint16_t addresses[] = {0x0000, 0x07FF, 0x6000, 0x7FFF, 0xFFFF};
    uint8_t values[] = {0x00, 0xFF, 0x7F, 0x80, 0xAA};
    
    for (int i = 0; i < 5; i++) {
        mockMemoryValue = values[i];
        MemoryReadCommand cmd(addresses[i]);
        auto future = cmd.getResult();
        
        cmd.execute();
        auto result = future.get();
        
        assert(result.address == addresses[i]);
        assert(result.value == values[i]);
        
        std::cout << "  ✓ Address 0x" << std::hex << addresses[i] 
                  << " -> 0x" << (int)values[i] << std::dec << std::endl;
    }
}

void testBinaryFormatting() {
    std::cout << "Testing binary formatting..." << std::endl;
    
    struct TestCase {
        uint8_t value;
        const char* expectedBinary;
    };
    
    TestCase cases[] = {
        {0x00, "00000000"},
        {0xFF, "11111111"},
        {0xAA, "10101010"},
        {0x55, "01010101"},
        {0x0F, "00001111"},
        {0xF0, "11110000"},
        {0x01, "00000001"},
        {0x80, "10000000"}
    };
    
    for (const auto& test : cases) {
        MemoryReadResult result;
        result.address = 0x0000;
        result.value = test.value;
        
        std::string json = result.toJson();
        std::string expectedBinary = std::string("\"binary\":\"") + test.expectedBinary + "\"";
        
        assert(json.find(expectedBinary) != std::string::npos);
        std::cout << "  ✓ 0x" << std::hex << (int)test.value 
                  << " -> " << test.expectedBinary << std::dec << std::endl;
    }
}

int main() {
    std::cout << "=== MemoryReadCommand Unit Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        testJsonFormat();
        std::cout << std::endl;
        
        testSuccessfulRead();
        std::cout << std::endl;
        
        testNoGameLoaded();
        std::cout << std::endl;
        
        testVariousAddresses();
        std::cout << std::endl;
        
        testBinaryFormatting();
        std::cout << std::endl;
        
        std::cout << "=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}