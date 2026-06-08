cmake_minimum_required(VERSION 3.16)
project(ungula_generic_lib_tests)

set(CMAKE_CXX_STANDARD 17)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

set(LIB_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

include_directories(
    ${LIB_SRC}
    ${CMAKE_CURRENT_LIST_DIR}/ArduinoStub
)

add_compile_definitions(
    # 
)

add_executable(test_ungula_core
    test_crc32.cpp
    test_preferences_layout.cpp
    test_pid.cpp
    test_queue.cpp
    test_string_utils.cpp
    test_types.cpp
    test_time_control.cpp
    test_time_format.cpp
    # Host backend for ungula::core::preferences::initStorage() — no-op
    # returning true, gated to non-MCU builds inside the .cpp so the file
    # is silent on ESP32. Without this the test_preferences_layout link
    # would fail to find initStorage on host.
    ${LIB_SRC}/ungula/core/preferences/platforms/host_preferences.cpp
)
target_link_libraries(test_ungula_core gtest_main)

add_library(check_preferences_alias_esp_platform OBJECT
    check_preferences_alias_dispatch.cpp
)
target_compile_definitions(check_preferences_alias_esp_platform PRIVATE ESP_PLATFORM=1)

add_library(check_preferences_alias_arduino_esp32 OBJECT
    check_preferences_alias_dispatch.cpp
)
target_compile_definitions(check_preferences_alias_arduino_esp32 PRIVATE ARDUINO_ARCH_ESP32=1)

add_library(check_preferences_alias_esp32_symbol OBJECT
    check_preferences_alias_dispatch.cpp
)
target_compile_definitions(check_preferences_alias_esp32_symbol PRIVATE ESP32=1)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_ungula_core)
