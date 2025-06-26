cmake_minimum_required(VERSION 3.31)
project(SmartHome)

set(CMAKE_CXX_STANDARD 23)

add_executable(SmartHome main.cpp smart_home_core.cpp)
