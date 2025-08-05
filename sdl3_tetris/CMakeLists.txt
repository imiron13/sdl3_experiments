cmake_minimum_required(VERSION 3.10)
project(sdl3_tetris)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    sdl3_tetris.cpp
)

find_package(SDL3 REQUIRED)
include_directories(${SDL3_INCLUDE_DIRS})
link_directories(${SDL3_LIBRARY_DIRS})
add_executable(sdl3_tetris ${SOURCES})
target_link_libraries(sdl3_tetris ${SDL3_LIBRARIES})
