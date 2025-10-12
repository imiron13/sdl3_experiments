cmake_minimum_required(VERSION 3.24)
project(nes_vgm_player LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Found ENV var")
endif()

# First try to find SDL3 installed on system (via find_package)
find_package(SDL3 CONFIG QUIET)

if (NOT SDL3_FOUND)
    message(STATUS "SDL3 not found on system, fetching and building SDL3...")

    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.20
    )
    FetchContent_MakeAvailable(SDL3)
endif()


set(SOURCES
    nes_vgm_player.cpp
    apu2A03.cpp
    apu2A03.h
)

add_executable(nes_vgm_player
    ${SOURCES}
)

target_link_libraries(nes_vgm_player PRIVATE SDL3::SDL3)
