cmake_minimum_required(VERSION 3.20)

project(dive LANGUAGES C)
enable_language(OBJC CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(lib/dcimgui EXCLUDE_FROM_ALL)

add_library(sokol STATIC lib/sokol/sokol.m)
target_include_directories(sokol PUBLIC lib/sokol)
target_compile_options(sokol PRIVATE -fobjc-arc)
target_link_libraries(sokol PUBLIC
    "-framework AppKit"
    "-framework QuartzCore"
    "-framework Metal")

add_executable(dive src/main.c)
target_compile_options(dive PRIVATE 
    -Wall -Wextra
    $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>)
target_link_options(dive PRIVATE 
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_libraries(dive PRIVATE imgui-docking sokol)
