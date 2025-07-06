# Qt Thread Interaction Proof of Concept

This proof of concept demonstrates safe cross-thread communication between a background thread and the Qt GUI thread using `QMetaObject::invokeMethod`.

## Overview

The PoC shows how a REST API server thread (or any background thread) can safely request the Qt GUI thread to perform actions without causing race conditions or deadlocks.

## Key Concepts Demonstrated

1. **Thread-Safe GUI Updates**: Using `QMetaObject::invokeMethod` with `Qt::QueuedConnection` to safely update GUI elements from a non-GUI thread.

2. **Bidirectional Communication**: Shows how to:
   - Send requests from background thread to GUI thread
   - Return results from GUI thread back to background thread

3. **Integration Pattern**: Demonstrates the pattern that can be used for REST API integration without modifying the existing emulator thread architecture.

## Files

- `main.cpp` - Main application entry point
- `MainWindow.h` - Qt MainWindow with GUI elements
- `MainWindow.cpp` - Implementation of thread-safe GUI updates
- `WorkerThread.h` - Background worker thread (simulates REST API server)
- `WorkerThread.cpp` - Implementation showing safe cross-thread calls
- `CMakeLists.txt` - Build configuration

## Building

```bash
mkdir build
cd build
cmake ..
make
./qt_thread_poc
```

## How It Works

1. The main thread runs the Qt event loop and GUI
2. A worker thread simulates a REST API server
3. The worker thread uses `QMetaObject::invokeMethod` to request GUI updates
4. Results are passed back to the worker thread via signals/slots or callbacks
5. All communication is thread-safe and non-blocking

## Integration with FCEUX

This pattern can be used for the REST API server to:
- Read emulator state safely
- Request emulator actions (pause, reset, load ROM)
- Update GUI elements (status indicators)
- Access game memory without race conditions

The key is that all actual emulator/GUI operations happen on their respective threads, with thread-safe message passing between them.