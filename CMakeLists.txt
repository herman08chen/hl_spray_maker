cmake_minimum_required(VERSION 3.20)

project(hl_spray_maker
        VERSION 1.0.0
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hl_spray_maker
        main.cpp
        image.cpp
        wad.cpp
)

# Keep strict warnings on GCC and Clang without overriding the selected build type.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hl_spray_maker PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wformat=2
            -Wnull-dereference
            -Wdouble-promotion
            -Wimplicit-fallthrough
    )
endif()

# libimagequant: prebuilt static library
add_library(imagequant STATIC IMPORTED GLOBAL)

set_target_properties(imagequant PROPERTIES
        IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libimagequant/lib/libimagequant_sys.a"
)

# Treat third-party headers as system headers.
target_include_directories(hl_spray_maker SYSTEM PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libimagequant/include"
)

find_package(Threads REQUIRED)

target_link_libraries(hl_spray_maker PRIVATE
        imagequant
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

if(UNIX)
    target_link_libraries(hl_spray_maker PRIVATE m)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
