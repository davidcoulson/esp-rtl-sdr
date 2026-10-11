# Standalone host unit tests (no ESP-IDF).
cmake_minimum_required(VERSION 3.16)
project(esp_rtl_sdr_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(esp_rtl_sdr_host_tests
    test_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
)

add_executable(esp_rtl_sdr_profile_tests
    test_profiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
)

# Same header built as Kconfig's librtlsdr IF mode would build it
add_executable(esp_rtl_sdr_if_mode_tests test_if_mode_librtlsdr.cpp)
target_compile_definitions(esp_rtl_sdr_if_mode_tests PRIVATE CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR=1)
target_include_directories(esp_rtl_sdr_if_mode_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

target_include_directories(esp_rtl_sdr_host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/p4_serial_smoke/main
)

target_include_directories(esp_rtl_sdr_profile_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

if(MSVC)
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_if_mode_tests PRIVATE /W4)
else()
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_if_mode_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME policy COMMAND esp_rtl_sdr_host_tests)
add_test(NAME profiles COMMAND esp_rtl_sdr_profile_tests)
add_test(NAME if_mode_librtlsdr COMMAND esp_rtl_sdr_if_mode_tests)
