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

add_executable(esp_rtl_sdr_multi_tests
    test_multi_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_multi.cpp
)

add_executable(esp_rtl_sdr_band_tests
    test_r820t2_band.cpp
)

target_include_directories(esp_rtl_sdr_band_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

add_executable(esp_rtl_sdr_v4_band_tests
    test_v4_band.cpp
)

target_include_directories(esp_rtl_sdr_v4_band_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

add_executable(esp_rtl_sdr_if_mode_tests
    test_r820t2_if_mode.cpp
)

target_include_directories(esp_rtl_sdr_if_mode_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

# r820t2_if_mode.hpp built as each Kconfig R820T2 IF mode would build it (no symbol = the default, A)
set(IF_MODE_BUILDS)
foreach(mode_pair "VendorFixed;" "Table;CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_TABLE=1"
        "Librtlsdr;CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR=1")
    list(GET mode_pair 0 mode)
    list(GET mode_pair 1 define)
    set(target esp_rtl_sdr_if_mode_build_${mode})
    add_executable(${target} test_if_mode_build.cpp)
    target_compile_definitions(${target} PRIVATE EXPECT_IF_MODE=${mode} ${define})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../private
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    list(APPEND IF_MODE_BUILDS ${mode})
endforeach()

add_executable(esp_rtl_sdr_usb_guard_tests
    test_usb_guard.cpp
)

target_include_directories(esp_rtl_sdr_usb_guard_tests PRIVATE
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

target_include_directories(esp_rtl_sdr_multi_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

if(MSVC)
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_multi_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_usb_guard_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_v4_band_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_band_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_if_mode_tests PRIVATE /W4)
else()
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_multi_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_usb_guard_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_v4_band_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_band_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_if_mode_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME policy COMMAND esp_rtl_sdr_host_tests)
add_test(NAME profiles COMMAND esp_rtl_sdr_profile_tests)
add_test(NAME multi_device COMMAND esp_rtl_sdr_multi_tests)
add_test(NAME usb_guard COMMAND esp_rtl_sdr_usb_guard_tests)
add_test(NAME v4_band COMMAND esp_rtl_sdr_v4_band_tests)
add_test(NAME r820t2_band COMMAND esp_rtl_sdr_band_tests)
add_test(NAME r820t2_if_mode COMMAND esp_rtl_sdr_if_mode_tests)
foreach(mode ${IF_MODE_BUILDS})
  add_test(NAME r820t2_if_mode_build_${mode} COMMAND esp_rtl_sdr_if_mode_build_${mode})
endforeach()
