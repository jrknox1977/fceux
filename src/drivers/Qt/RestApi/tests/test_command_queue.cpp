/**
 * Unit tests for REST API Command Queue
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include "../CommandQueue.h"
#include "../RestApiCommands.h"

// Mock command for testing
class MockCommand : public ApiCommand {
public:
    static std::atomic<int> executeCount;
    std::string commandName;
    bool shouldThrow;
    
    explicit MockCommand(const std::string& name = "MockCommand", bool throwException = false) 
        : commandName(name), shouldThrow(throwException) {}
    
    void execute() override {
        executeCount++;
        if (shouldThrow) {
            throw std::runtime_error("Test exception");
        }
    }
    
    const char* name() const override {
        return commandName.c_str();
    }
};

std::atomic<int> MockCommand::executeCount{0};

// Command with result for testing
class TestResultCommand : public ApiCommandWithResult<int> {
public:
    int value;
    
    explicit TestResultCommand(int val) : value(val) {}
    
    void execute() override {
        resultPromise.set_value(value);
    }
    
    const char* name() const override {
        return "TestResultCommand";
    }
};

// Test fixture
class CommandQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        MockCommand::executeCount = 0;
    }
};

// Basic queue operations
TEST_F(CommandQueueTest, PushPopSingleThread) {
    CommandQueue queue;
    
    // Test empty queue
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(queue.tryPop(), nullptr);
    
    // Push command
    auto cmd = std::make_unique<MockCommand>("test1");
    EXPECT_TRUE(queue.push(std::move(cmd)));
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);
    
    // Pop command
    auto popped = queue.tryPop();
    ASSERT_NE(popped, nullptr);
    EXPECT_STREQ(popped->name(), "test1");
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

// Test queue limits
TEST_F(CommandQueueTest, QueueFull) {
    CommandQueue queue(5);  // Small queue for testing
    
    // Fill queue
    for (int i = 0; i < 5; i++) {
        auto cmd = std::make_unique<MockCommand>("cmd" + std::to_string(i));
        EXPECT_TRUE(queue.push(std::move(cmd)));
    }
    
    EXPECT_EQ(queue.size(), 5);
    
    // Try to push to full queue
    auto cmd = std::make_unique<MockCommand>("overflow");
    EXPECT_FALSE(queue.push(std::move(cmd)));
    EXPECT_EQ(queue.size(), 5);
}

// Test null command handling
TEST_F(CommandQueueTest, NullCommand) {
    CommandQueue queue;
    
    std::unique_ptr<ApiCommand> nullCmd;
    EXPECT_FALSE(queue.push(std::move(nullCmd)));
    EXPECT_TRUE(queue.empty());
}

// Test clear operation
TEST_F(CommandQueueTest, Clear) {
    CommandQueue queue;
    
    // Add some commands
    for (int i = 0; i < 3; i++) {
        auto cmd = std::make_unique<MockCommand>();
        queue.push(std::move(cmd));
    }
    
    EXPECT_EQ(queue.size(), 3);
    
    // Clear queue
    queue.clear();
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

// Test concurrent access
TEST_F(CommandQueueTest, ConcurrentAccess) {
    CommandQueue queue;
    const int numThreads = 4;
    const int commandsPerThread = 100;
    
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::atomic<int> totalPushed{0};
    std::atomic<int> totalPopped{0};
    
    // Producer threads
    for (int t = 0; t < numThreads; t++) {
        producers.emplace_back([&queue, &totalPushed, t]() {
            for (int i = 0; i < commandsPerThread; i++) {
                auto cmd = std::make_unique<MockCommand>("thread" + std::to_string(t));
                if (queue.push(std::move(cmd))) {
                    totalPushed++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Consumer threads
    for (int t = 0; t < numThreads; t++) {
        consumers.emplace_back([&queue, &totalPopped]() {
            while (totalPopped < numThreads * commandsPerThread) {
                auto cmd = queue.tryPop();
                if (cmd) {
                    totalPopped++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    
    EXPECT_EQ(totalPushed, totalPopped);
    EXPECT_TRUE(queue.empty());
}

// Test command with result
TEST_F(CommandQueueTest, CommandWithResult) {
    auto cmd = std::make_unique<TestResultCommand>(42);
    auto future = cmd->getResult();
    
    // Execute command
    cmd->execute();
    
    // Get result
    ASSERT_EQ(future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_EQ(future.get(), 42);
}

// Test exception handling
TEST_F(CommandQueueTest, CommandException) {
    CommandQueue queue;
    
    auto cmd = std::make_unique<MockCommand>("throwing", true);
    queue.push(std::move(cmd));
    
    auto popped = queue.tryPop();
    ASSERT_NE(popped, nullptr);
    
    // Execute should throw
    EXPECT_THROW(popped->execute(), std::runtime_error);
}

// Performance test
TEST_F(CommandQueueTest, Performance) {
    CommandQueue queue;
    const int numCommands = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Push commands
    for (int i = 0; i < numCommands; i++) {
        auto cmd = std::make_unique<MockCommand>();
        queue.push(std::move(cmd));
    }
    
    // Pop and execute commands
    int processed = 0;
    while (auto cmd = queue.tryPop()) {
        cmd->execute();
        processed++;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    EXPECT_EQ(processed, numCommands);
    EXPECT_EQ(MockCommand::executeCount, numCommands);
    
    // Performance check: should be well under 1ms for 1000 commands
    double avgTimePerCommand = duration.count() / static_cast<double>(numCommands);
    std::cout << "Average time per command: " << avgTimePerCommand << " microseconds" << std::endl;
    EXPECT_LT(avgTimePerCommand, 1000.0);  // Less than 1ms per command
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}