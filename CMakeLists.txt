cmake_minimum_required(VERSION 3.9)
project(cpujumps)

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
    add_definitions("-DWIN32")
endif()

add_executable(cpujumps main.cpp)