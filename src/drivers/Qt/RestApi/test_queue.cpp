/**
 * Simple command queue test program
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "CommandQueue.h"
#include "RestApiCommands.h"

// Test command that increments a counter
class IncrementCommand : public ApiCommand {
public:
    static int counter;
    
    void execute() override {
        counter++;
        std::cout << "Executed IncrementCommand, counter = " << counter << std::endl;
    }
    
    const char* name() const override {
        return "IncrementCommand";
    }
};

int IncrementCommand::counter = 0;

// Test command with result
class AddCommand : public ApiCommandWithResult<int> {
private:
    int a, b;
    
public:
    AddCommand(int x, int y) : a(x), b(y) {}
    
    void execute() override {
        int result = a + b;
        std::cout << "Executed AddCommand: " << a << " + " << b << " = " << result << std::endl;
        resultPromise.set_value(result);
    }
    
    const char* name() const override {
        return "AddCommand";
    }
};

int main() {
    std::cout << "=== Command Queue Test ===" << std::endl;
    
    CommandQueue queue;
    
    // Test 1: Basic push/pop
    std::cout << "\nTest 1: Basic operations" << std::endl;
    assert(queue.empty());
    assert(queue.size() == 0);
    
    auto cmd1 = std::make_unique<IncrementCommand>();
    assert(queue.push(std::move(cmd1)));
    assert(!queue.empty());
    assert(queue.size() == 1);
    
    auto popped = queue.tryPop();
    assert(popped != nullptr);
    popped->execute();
    assert(IncrementCommand::counter == 1);
    assert(queue.empty());
    
    // Test 2: Multiple commands
    std::cout << "\nTest 2: Multiple commands" << std::endl;
    for (int i = 0; i < 5; i++) {
        auto cmd = std::make_unique<IncrementCommand>();
        queue.push(std::move(cmd));
    }
    assert(queue.size() == 5);
    
    while (!queue.empty()) {
        auto cmd = queue.tryPop();
        cmd->execute();
    }
    assert(IncrementCommand::counter == 6);
    
    // Test 3: Command with result
    std::cout << "\nTest 3: Command with result" << std::endl;
    auto addCmd = std::make_unique<AddCommand>(10, 20);
    auto future = addCmd->getResult();
    
    queue.push(std::move(addCmd));
    auto cmd = queue.tryPop();
    cmd->execute();
    
    assert(future.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    int result = future.get();
    assert(result == 30);
    std::cout << "Result: " << result << std::endl;
    
    // Test 3b: Command queue clear with pending promises
    std::cout << "\nTest 3b: Queue clear with pending promises" << std::endl;
    auto clearCmd1 = std::make_unique<AddCommand>(5, 10);
    auto future1 = clearCmd1->getResult();
    auto clearCmd2 = std::make_unique<AddCommand>(15, 20);
    auto future2 = clearCmd2->getResult();
    
    queue.push(std::move(clearCmd1));
    queue.push(std::move(clearCmd2));
    
    // Clear queue without executing commands
    queue.clear();
    
    // Futures should throw exception when accessed
    try {
        future1.get();
        assert(false); // Should not reach here
    } catch (const std::runtime_error& e) {
        std::cout << "Future1 correctly threw: " << e.what() << std::endl;
        assert(std::string(e.what()).find("cancelled") != std::string::npos);
    }
    
    try {
        future2.get();
        assert(false); // Should not reach here
    } catch (const std::runtime_error& e) {
        std::cout << "Future2 correctly threw: " << e.what() << std::endl;
        assert(std::string(e.what()).find("cancelled") != std::string::npos);
    }
    
    // Test 4: Concurrent access
    std::cout << "\nTest 4: Concurrent access" << std::endl;
    std::atomic<int> producerCount{0};
    std::atomic<int> consumerCount{0};
    
    // Producer thread
    std::thread producer([&queue, &producerCount]() {
        for (int i = 0; i < 100; i++) {
            auto cmd = std::make_unique<IncrementCommand>();
            if (queue.push(std::move(cmd))) {
                producerCount++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Consumer thread
    std::thread consumer([&queue, &consumerCount]() {
        while (consumerCount < 100) {
            auto cmd = queue.tryPop();
            if (cmd) {
                cmd->execute();
                consumerCount++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    
    producer.join();
    consumer.join();
    
    assert(producerCount == 100);
    assert(consumerCount == 100);
    std::cout << "Produced: " << producerCount << ", Consumed: " << consumerCount << std::endl;
    std::cout << "Final counter: " << IncrementCommand::counter << std::endl;
    
    // Test 5: Performance
    std::cout << "\nTest 5: Performance test" << std::endl;
    const int numCommands = 500;  // Keep under queue limit
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Push commands
    int pushed = 0;
    for (int i = 0; i < numCommands; i++) {
        auto cmd = std::make_unique<IncrementCommand>();
        if (queue.push(std::move(cmd))) {
            pushed++;
        }
    }
    std::cout << "Pushed " << pushed << " commands" << std::endl;
    
    // Pop and execute
    int executed = 0;
    while (auto cmd = queue.tryPop()) {
        cmd->execute();
        executed++;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    assert(executed == numCommands);
    std::cout << "Executed " << executed << " commands in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average: " << (duration.count() / static_cast<double>(numCommands)) 
              << " microseconds per command" << std::endl;
    
    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}