cmake_minimum_required(VERSION 3.16)
project(panelka LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# raylib: use an installed copy if asked, otherwise fetch the pinned release.
option(PANELKA_SYSTEM_RAYLIB "Use an installed raylib (5.5 or newer) instead of fetching it" OFF)
set(PANELKA_RAYLIB_TAG "6.0" CACHE STRING "raylib git tag to fetch")

if(PANELKA_SYSTEM_RAYLIB)
    find_package(raylib REQUIRED)
else()
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG ${PANELKA_RAYLIB_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(panelka
    src/main.cpp
    src/gen.cpp
    src/interior.cpp
    src/atlas.cpp)
target_link_libraries(panelka PRIVATE raylib)

if(MSVC)
    target_compile_definitions(panelka PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    target_compile_options(panelka PRIVATE /W3 /utf-8)
else()
    target_compile_options(panelka PRIVATE -Wall -Wextra)
endif()

# The game looks for assets/ next to the executable (or one level up).
add_custom_command(TARGET panelka POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:panelka>/assets)
