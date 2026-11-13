cmake_minimum_required(VERSION 3.20)

project(dive LANGUAGES C)
enable_language(OBJC CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(HOT "Build with hot-reloading" ON)
if (HOT)
    set(DIVE_LIB_TYPE SHARED)
else()
    set(DIVE_LIB_TYPE STATIC)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options($<$<CONFIG:Debug>:-fsanitize=undefined,address>)
add_link_options($<$<CONFIG:Debug>:-fsanitize=undefined,address>)

add_subdirectory(lib/dcimgui EXCLUDE_FROM_ALL)

add_library(sokol ${DIVE_LIB_TYPE} lib/sokol/sokol.m)
target_include_directories(sokol PUBLIC lib/sokol)
target_compile_definitions(sokol PUBLIC SOKOL_NO_ENTRY)
target_compile_options(sokol PRIVATE -fobjc-arc -mcpu=apple-m1)
target_link_libraries(sokol PUBLIC
    imgui-docking
    "-framework AppKit"
    "-framework QuartzCore"
    "-framework Metal")


add_library(noise STATIC lib/noise/FastNoiseLite.c)
target_include_directories(noise PUBLIC lib/noise)
target_compile_options(noise PRIVATE -mcpu=apple-m1 -fwrapv -fno-sanitize=signed-integer-overflow)


file(GLOB_RECURSE DIVE_SOURCES CONFIGURE_DEPENDS "src/*.c")
list(FILTER DIVE_SOURCES EXCLUDE REGEX "/src/host/")
add_executable(host src/host/main.c)
if(HOT)
    add_library(dive MODULE ${DIVE_SOURCES})
    target_compile_definitions(host PRIVATE HOT DIVE_PATH="$<TARGET_FILE:dive>")
else()
    add_library(dive STATIC ${DIVE_SOURCES})
    target_link_libraries(host PRIVATE dive)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT HAVE_IPO)
if(HAVE_IPO)
    set_property(TARGET dive PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

target_include_directories(host PRIVATE src)
target_compile_options(host PRIVATE -Wall -Wextra)
target_link_libraries(host PRIVATE sokol imgui-docking)

target_include_directories(dive PRIVATE src)
target_include_directories(dive PRIVATE lib/base)
target_compile_options(dive PRIVATE -Wall -Wextra -mcpu=apple-m1)
target_link_libraries(dive PRIVATE imgui-docking sokol noise)
