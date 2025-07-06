// Standalone test server to verify cpp-httplib integration
// Compile: g++ -std=c++11 test_server.cpp -pthread -o test_server

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <signal.h>

// Download from: https://github.com/yhirose/cpp-httplib/releases/download/v0.14.3/httplib.h
#include "httplib.h"

std::atomic<bool> g_running(true);

void signal_handler(int sig) {
    std::cout << "\nShutting down server..." << std::endl;
    g_running = false;
}

int main() {
    // Register signal handler for clean shutdown
    signal(SIGINT, signal_handler);
    
    // Create server with thread pool
    httplib::ThreadPool pool(8);
    httplib::Server svr;
    
    // Configure timeouts
    svr.set_read_timeout(5, 0);  // 5 seconds
    svr.set_write_timeout(5, 0); // 5 seconds
    
    // Setup endpoints
    svr.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"({
            "status": "running",
            "version": "1.0.0",
            "emulator": "FCEUX",
            "api": "REST"
        })", "application/json");
    });
    
    svr.Post("/api/pause", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Pause command received" << std::endl;
        res.set_content(R"({"success": true, "action": "pause"})", "application/json");
    });
    
    svr.Post("/api/unpause", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Unpause command received" << std::endl;
        res.set_content(R"({"success": true, "action": "unpause"})", "application/json");
    });
    
    svr.Post("/api/frame_advance", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Frame advance command received" << std::endl;
        res.set_content(R"({"success": true, "action": "frame_advance"})", "application/json");
    });
    
    svr.Get("/api/memory/:address", [](const httplib::Request& req, httplib::Response& res) {
        auto address = req.path_params.at("address");
        std::cout << "Memory read request for address: " << address << std::endl;
        
        // Simulate memory read
        res.set_content(R"({"address": ")" + address + R"(", "value": "0x00"})", "application/json");
    });
    
    // Error handler
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        const char* fmt = R"({"error": "Not Found", "path": "%s"})";
        char buf[256];
        snprintf(buf, sizeof(buf), fmt, req.path.c_str());
        res.set_content(buf, "application/json");
    });
    
    // Start server in a separate thread
    std::thread server_thread([&svr]() {
        std::cout << "Starting HTTP server on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        svr.listen("localhost", 8080);
    });
    
    // Simulate main application loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Shutdown
    svr.stop();
    server_thread.join();
    
    std::cout << "Server stopped" << std::endl;
    return 0;
}