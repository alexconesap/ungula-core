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
    test_queue.cpp
    test_string_utils.cpp
    test_types.cpp
    test_time_control.cpp
)
target_link_libraries(test_ungula_core gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_ungula_core)
