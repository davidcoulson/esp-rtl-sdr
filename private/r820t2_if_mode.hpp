#pragma once

/*
 * R820T2 IF filter / IF mode (Blog V3, Nooelec SMArt v5). Provenance per mapping is in the comments
 * below: A from this repo's vendor-DLL captures, B from a downstream fork's table, C a port of
 * librtlsdr's r82xx_set_bandwidth() whose values this repo's V4 and Nooelec captures also record.
 * B and C are downstream (davidcoulson/esp-rtl-sdr) deviations, not clean-room evidence.
 */

#if __has_include("sdkconfig.h")
#include "sdkconfig.h" /* CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_* */
#endif
#include "measured_tuner_bandwidth.hpp"
#include "rtl_profile.hpp"

#include <cstddef>
#include <cstdint>

/**
 * R820T2 IF filter (reg 0x0a/0x0b) and IF frequency for a sample rate on the Blog V3 and the Nooelec
 * SMArt v5, with the tuner bandwidth left automatic. Three mappings, picked at build time (Kconfig
 * ESP_RTL_SDR_ESP_R820T2_IF_MODE; docs/KCONFIG.md has the values and an A/B procedure):
 *
 *  - A, VendorFixed (default): the wide filter and a 3.57 MHz IF at every rate. This is what the
 *    cold start already leaves (the tuner reinit's d5/6b plus the demod IF restore), matching the
 *    vendor DLL on these two boards, so A adds no writes and changes nothing.
 *  - B, Table: the table a downstream fork shipped (davidcoulson/esp-rtl-sdr 59deff2).
 *  - C, Librtlsdr: librtlsdr's r82xx_set_bandwidth() with bandwidth = sample rate.
 *
 * For B and C, r820t2_rate_plan() turns the mapping into a MeasuredTunerBandwidthPlan that start
 * applies with run_bandwidth_program() (filter, PLL at RF + IF, demod IF with settle reads, band
 * front end), and hot retunes keep its IF.
 */
enum class R820T2IfMode : uint8_t {
    VendorFixed, /* A: rtl_r820t2_if_vendor_fixed(), the cold-start state (default) */
    Table,       /* B: rtl_r820t2_if_for_rate_table() */
    Librtlsdr,   /* C: rtl_r820t2_if_for_rate_librtlsdr(), r82xx_set_bandwidth() */
};

#if defined(CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR)
constexpr R820T2IfMode kR820T2IfModeBuild = R820T2IfMode::Librtlsdr;
#elif defined(CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_TABLE)
constexpr R820T2IfMode kR820T2IfModeBuild = R820T2IfMode::Table;
#else
constexpr R820T2IfMode kR820T2IfModeBuild = R820T2IfMode::VendorFixed;
#endif

inline const char *rtl_r820t2_if_mode_name(R820T2IfMode mode)
{
    switch (mode) {
    case R820T2IfMode::Table: return "table";
    case R820T2IfMode::Librtlsdr: return "librtlsdr";
    case R820T2IfMode::VendorFixed: break;
    }
    return "vendor_fixed";
}

/* reg0a: only bit 4 is meaningful here; r820t2_rate_plan() adds the board's other bits. */
struct R820T2IfSetting {
    uint8_t reg0a;
    uint8_t reg0b;
    uint32_t if_hz;
};

/**
 * B: the downstream fork's table. It is not what any librtlsdr computes: its 300-1500 kHz rows are the
 * commented-out "settings from Oldenburger" table in the librtlsdr/librtlsdr fork's
 * tuner_r82xx.c, and of its 1800-5000 kHz rows only 1800 (0xaf) and 2200 (0x8f) keep that table's
 * reg 0x0b, with every IF but 3000/5000 changed (1800: 1.60 MHz, not 1.40; 2200: 1.75 MHz, not
 * 1.60) and 3000/5000 at 0x6f instead of 0x8f/0x6b. The row is the first one at or above the rate
 * (that fork compares against the midpoint of neighbouring rows), and above 5 MS/s it falls
 * through to the osmocom 6/7/8 MHz settings.
 *
 * reg0a holds that fork's low-nibble values, but the fork writes reg 0x0a with mask 0x0f while the
 * downstream driver used osmocom's mask 0x10, so only bit 4 ever reached the tuner: clear up to
 * 5 MS/s, where osmocom sets it above 2.43 MS/s. In the downstream fork the table took 915 MHz
 * utility meters at 2048k on a Nooelec SMArt v5 from 0 to 5 decodes in 10 min (Linux rtl_433: 10),
 * but against a mismatched start state (the replayed c5/8f filter under a 3.57 MHz IF, because the
 * fork skipped the cold tuner reinit on the Nooelec), not against A.
 */
inline R820T2IfSetting rtl_r820t2_if_for_rate_table(uint32_t sample_rate_sps)
{
    constexpr uint32_t kBwKhz[] = {300, 450, 600, 900, 1100, 1200, 1300, 1500, 1800, 2200, 3000, 5000};
    constexpr uint8_t kReg0b[] = {0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xaf, 0x8f, 0x6f, 0x6f};
    constexpr uint32_t kIfKhz[] = {1700, 1650, 1600, 1500, 1400, 1350, 1320, 1270, 1600, 1750, 2000, 3570};
    const uint32_t bw_khz = sample_rate_sps / 1000u;
    if (bw_khz > 7000u) {
        return {0x10, 0x0b, 4570000u};
    }
    if (bw_khz > 6000u) {
        return {0x10, 0x2a, 4570000u};
    }
    if (bw_khz > 5000u) {
        return {0x10, 0x6b, 3570000u};
    }
    size_t i = 0;
    while (i + 1 < sizeof(kBwKhz) / sizeof(kBwKhz[0]) && bw_khz > kBwKhz[i]) {
        ++i;
    }
    return {static_cast<uint8_t>(i == 11 ? 0x00 : 0x0f), kReg0b[i], kIfKhz[i] * 1000u};
}

/**
 * osmocom librtlsdr's r82xx_set_bandwidth() (tuner_r82xx.c; rtl-sdr-blog's copy is identical),
 * with bw = sample rate as rtlsdr_set_sample_rate() passes it. Above 2.43 MHz (the widest
 * low-pass corner plus both high-pass steps) it uses the fixed 6/7/8 MHz settings with reg 0x0a
 * bit 4 set. Below, it starts from a 2.3 MHz IF, enables each high-pass step (reg 0x0b bit 5 for
 * 380 kHz, bit 6 for 350 kHz; a set bit means off) only while the rate exceeds what the low-pass
 * filter alone can pass, picks the narrowest low-pass corner that still exceeds what is left
 * (reg 0x0b bits 3:0 = 15 - index), and centres the IF on the resulting passband.
 *
 * The same values appear in hardcoreerik/esp-rtl-sdr's own vendor-DLL captures (docs/captures,
 * 2026-10-01): the Blog V4/V4L rate matrix at every accepted rate, and the Nooelec v5's explicit
 * tuner-bandwidth choices at 2.4 MS/s (200k/300k, 500k, 1M, 1.8M, 2.4M). On a cold open the same
 * DLL leaves the V3c and the Nooelec at rtl_r820t2_if_vendor_fixed() instead.
 */
inline R820T2IfSetting rtl_r820t2_if_for_rate_librtlsdr(uint32_t sample_rate_sps)
{
    constexpr int64_t kLowPassHz[] = {1700000, 1600000, 1550000, 1450000, 1200000,
                                      900000,  700000,  550000,  450000,  350000};
    constexpr int64_t kHighPass1Hz = 350000;
    constexpr int64_t kHighPass2Hz = 380000;
    constexpr size_t kLowPassCount = sizeof(kLowPassHz) / sizeof(kLowPassHz[0]);
    int64_t bw = sample_rate_sps;
    if (bw > 7000000) {
        return {0x10, 0x0b, 4570000u};
    }
    if (bw > 6000000) {
        return {0x10, 0x2a, 4570000u};
    }
    if (bw > kLowPassHz[0] + kHighPass1Hz + kHighPass2Hz) {
        return {0x10, 0x6b, 3570000u};
    }
    uint8_t reg0b = 0x80;
    int64_t if_hz = 2300000;
    int64_t real_bw = 0;
    if (bw > kLowPassHz[0] + kHighPass1Hz) {
        bw -= kHighPass2Hz;
        if_hz += kHighPass2Hz;
        real_bw += kHighPass2Hz;
    } else {
        reg0b |= 0x20;
    }
    if (bw > kLowPassHz[0]) {
        bw -= kHighPass1Hz;
        if_hz += kHighPass1Hz;
        real_bw += kHighPass1Hz;
    } else {
        reg0b |= 0x40;
    }
    /* bw <= kLowPassHz[0] here, so the first corner below it is at index >= 1 */
    size_t i = 1;
    while (i < kLowPassCount && bw <= kLowPassHz[i]) {
        ++i;
    }
    --i;
    reg0b = static_cast<uint8_t>(reg0b | (15u - i));
    real_bw += kLowPassHz[i];
    if_hz -= real_bw / 2;
    return {0x00, reg0b, static_cast<uint32_t>(if_hz)};
}

/**
 * What the RTL-SDR Blog 1.4.0 DLL leaves on a Blog V3c (0a/0b = d5/6b) and a Nooelec v5 (d3/6b)
 * after a cold open at any accepted rate: the wide filter and a 3.57 MHz IF, the rate ignored
 * (hardcoreerik/esp-rtl-sdr docs/captures/vendor_dll_rate_matrix_2026-10-01.md, two identical runs
 * per dongle). This driver's cold start ends in the same state (d5/6b, 3.57 MHz; the DLL's Nooelec
 * has 3 in the low nibble). The DLL's behaviour on a hot rate change was not captured.
 * The driver never applies this as a plan: the cold start already ends here.
 */
inline R820T2IfSetting rtl_r820t2_if_vendor_fixed(uint32_t sample_rate_sps)
{
    (void)sample_rate_sps;
    return {0x10, 0x6b, 3570000u};
}

inline R820T2IfSetting rtl_r820t2_if_for_rate(uint32_t sample_rate_sps,
                                              R820T2IfMode mode = kR820T2IfModeBuild)
{
    switch (mode) {
    case R820T2IfMode::Librtlsdr: return rtl_r820t2_if_for_rate_librtlsdr(sample_rate_sps);
    case R820T2IfMode::VendorFixed: return rtl_r820t2_if_vendor_fixed(sample_rate_sps);
    case R820T2IfMode::Table: break;
    }
    return rtl_r820t2_if_for_rate_table(sample_rate_sps);
}

/** RTL2832 demod IF word (page 1 regs 0x19..0x1b), librtlsdr's rtlsdr_set_if_freq() arithmetic. */
inline uint32_t rtl_demod_if_word(uint32_t if_hz, uint32_t xtal_hz)
{
    const int64_t v = (static_cast<int64_t>(if_hz) << 22) / static_cast<int64_t>(xtal_hz);
    return static_cast<uint32_t>(-v) & 0x3fffffu;
}

/**
 * B or C as a plan for run_bandwidth_program(), or false to leave the cold-start state (A, any other
 * profile, or the direct-sampling route below 24 MHz, where the tuner is bypassed). reg 0x0a is the
 * board's captured base (Nooelec c3, V3 c5, as in measured_tuner_bandwidth_plan) with bit 4 from the
 * mapping; reg 0x0b is the mapping's value, whose bit 4 is always clear.
 */
inline bool r820t2_rate_plan(RtlProfileId profile, uint32_t rate_sps, uint32_t rf_hz,
                             MeasuredTunerBandwidthPlan *out,
                             R820T2IfMode mode = kR820T2IfModeBuild)
{
    if (out == nullptr || mode == R820T2IfMode::VendorFixed) return false;
    if (profile != RtlProfileId::BlogV3 && profile != RtlProfileId::NooelecSmartV5) return false;
    if (rtl_profile_uses_v3_direct_sampling(profile, rf_hz)) return false;
    const R820T2IfSetting s = rtl_r820t2_if_for_rate(rate_sps, mode);
    const uint8_t base = profile == RtlProfileId::NooelecSmartV5 ? 0xc3 : 0xc5;
    const uint32_t word = rtl_demod_if_word(s.if_hz, ESP_RTL_SDR_XTAL_HZ);
    *out = {0u, s.if_hz, static_cast<uint8_t>(base | (s.reg0a & 0x10u)), s.reg0b,
            static_cast<uint8_t>((word >> 16) & 0x3fu), static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)};
    return true;
}
