cmake_minimum_required(VERSION 3.16)
project(CS2_ESP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CS2_ESP_RETAIL "Enable fixed-day license expiry (first run + N days)" OFF)
set(CS2_ESP_EXPIRY_DAYS 3 CACHE STRING "Valid days after first launch (retail builds)")
set(CS2_ESP_EXPIRY_SECONDS 0 CACHE STRING "If >0, validity in seconds after first launch (overrides days)")
option(CS2_ESP_LICENSE_VARIANT_TEST "Separate license store for test/trial retail builds" OFF)
option(CS2_ESP_LICENSE_VARIANT_TRIAL_ESP "5h ESP-only trial: machine-bound license file" OFF)
option(CS2_ESP_ESP_ONLY "Compile without memory aim (wallhack-only build)" OFF)

add_executable(CS2_ESP WIN32
    src/main.cpp
    src/license/expiry.cpp
    src/license/machine_id.cpp
    src/memory/process.cpp
    src/sdk/entity.cpp
    src/esp/esp.cpp
    src/render/overlay.cpp
    src/render/color_palette.cpp
    src/render/color_picker_panel.cpp
    src/stealth/stealth.cpp
    src/config/runtime_config.cpp
)

if(NOT CS2_ESP_ESP_ONLY)
    target_sources(CS2_ESP PRIVATE src/aim/soft_aim.cpp)
endif()

target_compile_definitions(CS2_ESP PRIVATE $<$<CONFIG:Debug>:CS2_ESP_CONSOLE>)
if(CS2_ESP_RETAIL)
    target_compile_definitions(CS2_ESP PRIVATE CS2_ESP_RETAIL)
    if(CS2_ESP_EXPIRY_SECONDS GREATER 0)
        target_compile_definitions(CS2_ESP PRIVATE CS2_ESP_EXPIRY_SECONDS=${CS2_ESP_EXPIRY_SECONDS})
    else()
        target_compile_definitions(CS2_ESP PRIVATE CS2_ESP_EXPIRY_DAYS=${CS2_ESP_EXPIRY_DAYS})
    endif()
    if(CS2_ESP_LICENSE_VARIANT_TEST)
        target_compile_definitions(CS2_ESP PRIVATE CS2_ESP_LICENSE_VARIANT_TEST)
    endif()
    if(CS2_ESP_LICENSE_VARIANT_TRIAL_ESP)
        target_compile_definitions(CS2_ESP PRIVATE CS2_ESP_LICENSE_VARIANT_TRIAL_ESP)
    endif()
endif()
if(CS2_ESP_ESP_ONLY)
    target_compile_definitions(CS2_ESP PRIVATE CS2_ESP_ESP_ONLY)
endif()

target_include_directories(CS2_ESP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk
)

target_link_libraries(CS2_ESP PRIVATE
    d2d1
    dwrite
    user32
    gdi32
    kernel32
    shell32
    ole32
)

if(MSVC)
    target_compile_options(CS2_ESP PRIVATE /W4 /permissive-)
endif()
