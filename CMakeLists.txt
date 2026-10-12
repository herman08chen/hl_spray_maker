
cmake_minimum_required(VERSION 3.20)

project(hl_spray_maker
        VERSION 1.0.0
        LANGUAGES CXX
)

include(CTest)

# The project uses C++23 facilities (including std::print); no C++26 feature is required.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIQ_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libimagequant/include"
)

if(EMSCRIPTEN)
    add_executable(spray_maker
            web_bindings.cpp
            image.cpp
            wad.cpp
    )

    add_library(imagequant STATIC IMPORTED GLOBAL)

    set_target_properties(imagequant PROPERTIES
            IMPORTED_LOCATION
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libimagequant/wasm/libimagequant_sys.a"
    )

    target_include_directories(spray_maker SYSTEM PRIVATE
            "${LIQ_INCLUDE_DIR}"
    )

    target_link_libraries(spray_maker PRIVATE imagequant)

    target_compile_options(spray_maker PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wformat=2
    )

    target_link_options(spray_maker PRIVATE
            --no-entry
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createModule"
            "-sENVIRONMENT=web"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sEXPORTED_FUNCTIONS=['_malloc','_free','_create_wad','_free_wad','_wad_last_error','_expose_wasm_memory']"
            "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','wasmMemory']"
    )

    set_target_properties(spray_maker PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
    )

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/web")

    configure_file(web/index.html
            "${CMAKE_BINARY_DIR}/web/index.html" COPYONLY)
    configure_file(web/style.css
            "${CMAKE_BINARY_DIR}/web/style.css" COPYONLY)
    configure_file(web/app.js
            "${CMAKE_BINARY_DIR}/web/app.js" COPYONLY)

else()
    add_executable(hl_spray_maker
            main.cpp
            image.cpp
            wad.cpp
    )

    add_library(imagequant STATIC IMPORTED GLOBAL)

    set_target_properties(imagequant PROPERTIES
            IMPORTED_LOCATION
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libimagequant/lib/libimagequant_sys.a"
    )

    target_include_directories(hl_spray_maker SYSTEM PRIVATE
            "${LIQ_INCLUDE_DIR}"
    )

    target_link_libraries(hl_spray_maker PRIVATE
            imagequant
            ${CMAKE_DL_LIBS}
    )

    if(UNIX)
        target_link_libraries(hl_spray_maker PRIVATE m)
    endif()

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

    if(BUILD_TESTING)
        add_executable(resolution_tests resolution_tests.cpp image.cpp wad.cpp)
        target_include_directories(resolution_tests SYSTEM PRIVATE "${LIQ_INCLUDE_DIR}")
        target_link_libraries(resolution_tests PRIVATE imagequant ${CMAKE_DL_LIBS})
        if(UNIX)
            target_link_libraries(resolution_tests PRIVATE m)
        endif()
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(resolution_tests PRIVATE
                    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
                    -Wsign-conversion -Wformat=2 -Wnull-dereference
                    -Wdouble-promotion -Wimplicit-fallthrough
            )
        endif()
        add_test(NAME resolution_tests COMMAND resolution_tests)
    endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
