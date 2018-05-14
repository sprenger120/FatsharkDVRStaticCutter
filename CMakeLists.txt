cmake_minimum_required(VERSION 3.7)
project(dvrsnippy)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-O2 -g -march=native -std=c++11 -Wall")

set(SOURCE_FILES main.cpp)
add_executable(dvrsnippy ${SOURCE_FILES})

target_link_libraries(dvrsnippy ${OpenCV_LIBS} )