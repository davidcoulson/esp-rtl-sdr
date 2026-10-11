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

# The header built as each Kconfig R820T2 IF mode would build it (no symbol = the default)
set(IF_MODE_TESTS)
foreach(mode_pair "Table;" "Librtlsdr;CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR=1"
        "VendorFixed;CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_VENDOR_FIXED=1")
    list(GET mode_pair 0 mode)
    list(GET mode_pair 1 define)
    set(target esp_rtl_sdr_if_mode_${mode})
    add_executable(${target} test_if_mode_build.cpp)
    target_compile_definitions(${target} PRIVATE EXPECT_IF_MODE=${mode} ${define})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../private
    )
    list(APPEND IF_MODE_TESTS ${mode})
endforeach()

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

foreach(mode ${IF_MODE_TESTS})
  if(MSVC)
    target_compile_options(esp_rtl_sdr_if_mode_${mode} PRIVATE /W4)
  else()
    target_compile_options(esp_rtl_sdr_if_mode_${mode} PRIVATE -Wall -Wextra)
  endif()
endforeach()

if(MSVC)
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE /W4)
else()
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME policy COMMAND esp_rtl_sdr_host_tests)
add_test(NAME profiles COMMAND esp_rtl_sdr_profile_tests)
foreach(mode ${IF_MODE_TESTS})
  add_test(NAME if_mode_${mode} COMMAND esp_rtl_sdr_if_mode_${mode})
endforeach()
