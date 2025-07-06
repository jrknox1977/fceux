cmake_minimum_required(VERSION 3.16)
project(qt_thread_poc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

add_executable(qt_thread_poc
    main.cpp
    MainWindow.cpp
    MainWindow.h
    WorkerThread.cpp
    WorkerThread.h
)

target_link_libraries(qt_thread_poc
    Qt5::Core
    Qt5::Widgets
)

# If Qt5 not found, try Qt6
if(NOT Qt5_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
    target_link_libraries(qt_thread_poc
        Qt6::Core
        Qt6::Widgets
    )
endif()