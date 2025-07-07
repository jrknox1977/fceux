
cmake_minimum_required(VERSION 3.8)

project(fceux)

# Feature options
option(REST_API "Enable REST API server support" OFF)

add_subdirectory( src )

