cmake_minimum_required(VERSION 3.24)
project(sdl3_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    message(STATUS "Found ENV var")
endif()

# First try to find SDL3 installed on system (via find_package)
find_package(SDL3 3.2.0 CONFIG QUIET)
find_package(SDL3_image CONFIG QUIET)

if (NOT SDL3_FOUND)
    message(STATUS "SDL3 not found on system, fetching and building SDL3...")

    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.24
    )
    FetchContent_MakeAvailable(SDL3)
endif()

if (NOT SDL3_image_FOUND)
    message(STATUS "SDL3_image not found on system, fetching and building SDL3_image...")

    include(FetchContent)
    FetchContent_Declare(
        SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-3.2.4
    )
    FetchContent_MakeAvailable(SDL3_image)
endif()

set(SOURCES
    sdl3_chess.cpp
)

if(WIN32)
    add_executable(sdl3_chess WIN32
        ${SOURCES}
    )
else()
    add_executable(sdl3_chess
        ${SOURCES}
    )
endif()

target_link_libraries(sdl3_chess PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
