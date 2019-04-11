cmake_minimum_required(VERSION 2.9)

project(os_find)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # For ccls

add_executable(find src/main.cpp)
