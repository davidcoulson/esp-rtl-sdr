/*
 * Built once per Kconfig ESP_RTL_SDR_ESP_R820T2_IF_MODE choice, with the CONFIG_ symbol sdkconfig.h
 * would define and EXPECT_IF_MODE naming the mode it must select: the default argument of
 * rtl_r820t2_if_for_rate() (what src/esp_rtl_sdr.cpp calls) must follow it.
 */
#include "rtl_profile.hpp"

#include <cstdio>
#include <initializer_list>

#define IF_MODE_STR2(x) #x
#define IF_MODE_STR(x) IF_MODE_STR2(x)

int main(void)
{
    constexpr R820T2IfMode kExpected = R820T2IfMode::EXPECT_IF_MODE;
    int failed = 0;
    if (kR820T2IfModeBuild != kExpected) {
        std::printf("FAIL build mode is not %s\n", IF_MODE_STR(EXPECT_IF_MODE));
        ++failed;
    }
    for (uint32_t rate : {250000u, 1024000u, 2048000u, 2560000u}) {
        const R820T2IfSetting d = rtl_r820t2_if_for_rate(rate);
        const R820T2IfSetting e = rtl_r820t2_if_for_rate(rate, kExpected);
        if (d.reg0a != e.reg0a || d.reg0b != e.reg0b || d.if_hz != e.if_hz) {
            std::printf("FAIL default mapping at %u S/s is not the expected mode\n",
                        static_cast<unsigned>(rate));
            ++failed;
        }
    }
    std::printf("RESULT if_mode_build %s failed=%d\n", IF_MODE_STR(EXPECT_IF_MODE), failed);
    return failed == 0 ? 0 : 1;
}
