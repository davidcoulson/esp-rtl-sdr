/*
 * Built with -DCONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR=1, as sdkconfig.h defines it when
 * Kconfig ESP_RTL_SDR_ESP_R820T2_IF_MODE picks librtlsdr: the default argument of
 * rtl_r820t2_if_for_rate() (what src/esp_rtl_sdr.cpp calls) must follow it.
 */
#include "rtl_profile.hpp"

#include <cstdio>
#include <initializer_list>

int main(void)
{
    int failed = 0;
    if (kR820T2IfModeBuild != R820T2IfMode::Librtlsdr) {
        std::printf("FAIL build mode is not Librtlsdr\n");
        ++failed;
    }
    for (uint32_t rate : {250000u, 1024000u, 2048000u, 2560000u}) {
        const R820T2IfSetting d = rtl_r820t2_if_for_rate(rate);
        const R820T2IfSetting l = rtl_r820t2_if_for_rate_librtlsdr(rate);
        if (d.reg0a != l.reg0a || d.reg0b != l.reg0b || d.if_hz != l.if_hz) {
            std::printf("FAIL default mapping at %u S/s is not librtlsdr\n", static_cast<unsigned>(rate));
            ++failed;
        }
    }
    const R820T2IfSetting s = rtl_r820t2_if_for_rate(2048000u);
    if (s.reg0b != 0xaf || s.if_hz != 1625000u) {
        std::printf("FAIL 2048k: %02x / %u\n", s.reg0b, static_cast<unsigned>(s.if_hz));
        ++failed;
    }
    std::printf("RESULT if_mode_librtlsdr failed=%d\n", failed);
    return failed == 0 ? 0 : 1;
}
