/*
 * R820T2 IF filter / IF modes (private/r820t2_if_mode.hpp): each mapping against an independent
 * source, and the plan start applies for modes B and C. No USB / FreeRTOS.
 */
#include "esp_rtl_sdr.h"
#include "r820t2_if_mode.hpp"

#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <string_view>

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_U(a, b)                                                      \
    do {                                                                       \
        const auto _a = (a);                                                   \
        const auto _b = (b);                                                   \
        if (_a != _b) {                                                        \
            std::printf("FAIL %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, \
                        #a, (unsigned)_a, #b, (unsigned)_b);                   \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

/*
 * Expected librtlsdr values, produced by compiling rtl-sdr-blog's own r82xx_set_bandwidth()
 * (src/tuner_r82xx.c @ aed0ea1, byte-identical to osmocom rtl-sdr @ 797f814) with bw = rate and a
 * stub that records the reg 0x0a/0x0b values, and its rtlsdr_set_if_freq() arithmetic (double
 * division, 28.8 MHz xtal) for the demod word. Not derived from rtl_profile.hpp.
 */
struct R820T2IfGolden {
    uint32_t rate;
    uint8_t reg0a;
    uint8_t reg0b;
    uint32_t if_hz;
    uint32_t demod_word;
};

static const R820T2IfGolden kLibrtlsdrIfGolden[] = {
    {       0u, 0x00u, 0xe6u, 2125000u, 0x3b471du},
    {  225001u, 0x00u, 0xe6u, 2125000u, 0x3b471du},
    {  250000u, 0x00u, 0xe6u, 2125000u, 0x3b471du},
    {  300000u, 0x00u, 0xe6u, 2125000u, 0x3b471du},
    {  350000u, 0x00u, 0xe6u, 2125000u, 0x3b471du},
    {  350001u, 0x00u, 0xe7u, 2075000u, 0x3b638fu},
    {  900001u, 0x00u, 0xebu, 1700000u, 0x3c38e4u},
    {  960000u, 0x00u, 0xebu, 1700000u, 0x3c38e4u},
    { 1024000u, 0x00u, 0xebu, 1700000u, 0x3c38e4u},
    { 1200001u, 0x00u, 0xecu, 1575000u, 0x3c8000u},
    { 1400000u, 0x00u, 0xecu, 1575000u, 0x3c8000u},
    { 1700000u, 0x00u, 0xefu, 1450000u, 0x3cc71du},
    { 1700001u, 0x00u, 0xacu, 1750000u, 0x3c1c72u},
    { 1800000u, 0x00u, 0xacu, 1750000u, 0x3c1c72u},
    { 2048000u, 0x00u, 0xafu, 1625000u, 0x3c638fu},
    { 2050000u, 0x00u, 0xafu, 1625000u, 0x3c638fu},
    { 2050001u, 0x00u, 0xcfu, 1640000u, 0x3c5b06u},
    { 2400000u, 0x00u, 0x8fu, 1815000u, 0x3bf778u},
    { 2430000u, 0x00u, 0x8fu, 1815000u, 0x3bf778u},
    { 2430001u, 0x10u, 0x6bu, 3570000u, 0x381112u},
    { 2560000u, 0x10u, 0x6bu, 3570000u, 0x381112u},
    { 3200000u, 0x10u, 0x6bu, 3570000u, 0x381112u},
    { 6000000u, 0x10u, 0x6bu, 3570000u, 0x381112u},
    { 6000001u, 0x10u, 0x2au, 4570000u, 0x35d82eu},
    { 7000000u, 0x10u, 0x2au, 4570000u, 0x35d82eu},
    { 7000001u, 0x10u, 0x0bu, 4570000u, 0x35d82eu},
    { 8000000u, 0x10u, 0x0bu, 4570000u, 0x35d82eu},
};

static void test_r820t2_if_librtlsdr(void)
{
    for (const R820T2IfGolden &g : kLibrtlsdrIfGolden) {
        const int failed_before = g_failed;
        const R820T2IfSetting s = rtl_r820t2_if_for_rate(g.rate, R820T2IfMode::Librtlsdr);
        EXPECT_EQ_U(s.reg0a, g.reg0a);
        EXPECT_EQ_U(s.reg0b, g.reg0b);
        EXPECT_EQ_U(s.if_hz, g.if_hz);
        EXPECT_EQ_U(rtl_demod_if_word(s.if_hz, 28800000u), g.demod_word);
        /* the caller's masks: 0x0b bit 4 is never asked for, 0x0a only ever bit 4 */
        EXPECT_EQ_U(s.reg0b & 0x10u, 0u);
        EXPECT_EQ_U(s.reg0a & ~0x10u, 0u);
        if (g_failed != failed_before) {
            std::printf("  ^ librtlsdr IF mapping at %u S/s\n", static_cast<unsigned>(g.rate));
        }
    }
}

/*
 * Vendor DLL (RTL-SDR Blog 1.4.0) register captures from hardcoreerik/esp-rtl-sdr @ a5dc9d8,
 * docs/captures/rate_matrix_2026-10-01/, the run1 CSVs (run2 identical): a black-box source independent
 * of both the librtlsdr C and the port. The DLL's IF column is the demod word converted back, so it
 * reads a few Hz low (2124996 for 2125000); the word itself is compared exactly.
 */
struct R820T2VendorRow {
    uint32_t rate;
    uint8_t reg0a; /* only bit 4 is compared: the caller writes 0x0a with mask 0x10 */
    uint8_t reg0b;
    uint32_t if_hz;
    uint32_t demod_word;
};

/* blog_v4_run1.csv: the V4 follows the rate */
static const R820T2VendorRow kVendorV4RateMatrix[] = {
    { 250000u, 0xc5u, 0xe6u, 2124996u, 0x3b471du}, { 256000u, 0xc5u, 0xe6u, 2124996u, 0x3b471du},
    { 960000u, 0xc5u, 0xebu, 1699997u, 0x3c38e4u}, {1024000u, 0xc5u, 0xebu, 1699997u, 0x3c38e4u},
    {1400000u, 0xc5u, 0xecu, 1575000u, 0x3c8000u}, {1800000u, 0xc5u, 0xacu, 1749998u, 0x3c1c72u},
    {1920000u, 0xc5u, 0xaeu, 1674996u, 0x3c471du}, {2000000u, 0xc5u, 0xafu, 1624995u, 0x3c638fu},
    {2048000u, 0xc5u, 0xafu, 1624995u, 0x3c638fu}, {2400000u, 0xc5u, 0x8fu, 1814996u, 0x3bf778u},
    {2560000u, 0xd5u, 0x6bu, 3569994u, 0x381112u}, {2800000u, 0xd5u, 0x6bu, 3569994u, 0x381112u},
    {2880000u, 0xd5u, 0x6bu, 3569994u, 0x381112u}, {3200000u, 0xd5u, 0x6bu, 3569994u, 0x381112u},
};

/* nooelec_v5_run1.csv (d3/6b) and blog_v3c_run1.csv (d5/6b): same at every accepted rate */
static const uint32_t kVendorAcceptedRates[] = {250000u,  256000u,  960000u,  1024000u, 1400000u,
                                                1800000u, 1920000u, 2000000u, 2048000u, 2400000u,
                                                2560000u, 2800000u, 2880000u, 3200000u};

/* nooelec_v5_2026-09-30.md, explicit tuner bandwidth at 2.4 MS/s (reg0a c3): width -> 0b / IF word */
static const R820T2VendorRow kVendorNooelecBandwidth[] = {
    { 200000u, 0xc3u, 0xe6u, 2125000u, 0x3b471du}, { 300000u, 0xc3u, 0xe6u, 2125000u, 0x3b471du},
    { 500000u, 0xc3u, 0xe8u, 2025000u, 0x3b8000u}, {1000000u, 0xc3u, 0xebu, 1700000u, 0x3c38e4u},
    {1800000u, 0xc3u, 0xacu, 1750000u, 0x3c1c72u}, {2400000u, 0xc3u, 0x8fu, 1815000u, 0x3bf778u},
};

static void test_r820t2_if_vendor_captures(void)
{
    /* librtlsdr mode = the DLL on the V4 at every rate it accepts */
    for (const R820T2VendorRow &v : kVendorV4RateMatrix) {
        const int failed_before = g_failed;
        const R820T2IfSetting s = rtl_r820t2_if_for_rate(v.rate, R820T2IfMode::Librtlsdr);
        EXPECT_EQ_U(s.reg0a & 0x10u, v.reg0a & 0x10u);
        EXPECT_EQ_U(s.reg0b, v.reg0b);
        EXPECT_EQ_U(rtl_demod_if_word(s.if_hz, 28800000u), v.demod_word);
        EXPECT_TRUE(s.if_hz >= v.if_hz && s.if_hz - v.if_hz < 10u);
        if (g_failed != failed_before) {
            std::printf("  ^ librtlsdr vs vendor V4 rate matrix at %u S/s\n", static_cast<unsigned>(v.rate));
        }
    }
    /* and the DLL's Nooelec explicit bandwidths (the same function, bw = width) */
    for (const R820T2VendorRow &v : kVendorNooelecBandwidth) {
        const R820T2IfSetting s = rtl_r820t2_if_for_rate(v.rate, R820T2IfMode::Librtlsdr);
        EXPECT_EQ_U(s.reg0b, v.reg0b);
        EXPECT_EQ_U(s.if_hz, v.if_hz);
        EXPECT_EQ_U(rtl_demod_if_word(s.if_hz, 28800000u), v.demod_word);
    }
    /* vendor-fixed mode = the DLL's cold open on the V3c (d5/6b) and Nooelec (d3/6b) */
    for (uint32_t rate : kVendorAcceptedRates) {
        const R820T2IfSetting s = rtl_r820t2_if_for_rate(rate, R820T2IfMode::VendorFixed);
        EXPECT_EQ_U(s.reg0a & 0x10u, 0xd3u & 0x10u);
        EXPECT_EQ_U(s.reg0a & 0x10u, 0xd5u & 0x10u);
        EXPECT_EQ_U(s.reg0b, 0x6bu);
        EXPECT_EQ_U(rtl_demod_if_word(s.if_hz, 28800000u), 0x381112u);
        /* LO = RF + 3.57 MHz, the IF the R820T2 profiles' cold PLL math already uses */
        EXPECT_EQ_U(s.if_hz, kBlogV3DemodIfHz);
    }
    /* the replayed init's c5/8f (2.2 MHz filter) under a 3.57 MHz IF is none of the three */
    for (R820T2IfMode m : {R820T2IfMode::Table, R820T2IfMode::Librtlsdr, R820T2IfMode::VendorFixed}) {
        for (uint32_t rate : kVendorAcceptedRates) {
            const R820T2IfSetting s = rtl_r820t2_if_for_rate(rate, m);
            EXPECT_TRUE(!(s.reg0b == 0x8fu && s.if_hz == 3570000u));
        }
    }
}

/*
 * The driver table's provenance: the commented-out "settings from Oldenburger" table in the
 * librtlsdr/librtlsdr fork's src/tuner_r82xx.c @ cd36c28 (rows in kHz, reg 0x0a, reg 0x0b, IF kHz).
 */
static const uint32_t kOldenburgerBwKhz[] = {300, 450, 600, 900, 1100, 1200, 1300, 1500, 1800, 2200, 3000, 5000};
static const uint8_t kOldenburger0a[] = {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0e, 0x0f, 0x0f, 0x04, 0x0b};
static const uint8_t kOldenburger0b[] = {0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xaf, 0x8f, 0x8f, 0x6b};
static const uint32_t kOldenburgerIfKhz[] = {1700, 1650, 1600, 1500, 1400, 1350, 1320, 1270, 1400, 1600, 2000, 3570};

static void test_r820t2_if_table(void)
{
    /* each row covers rates above the previous row up to and including its own bandwidth */
    for (size_t i = 0; i < std::size(kOldenburgerBwKhz); ++i) {
        const uint32_t lo_khz = i == 0 ? 0u : kOldenburgerBwKhz[i - 1] + 1u;
        for (uint32_t khz : {lo_khz, kOldenburgerBwKhz[i]}) {
            const int failed_before = g_failed;
            const R820T2IfSetting s = rtl_r820t2_if_for_rate(khz * 1000u, R820T2IfMode::Table);
            /* only bit 4 is written (the fork's low nibble is dropped by the 0x10 mask), and like
             * Oldenburger's values the table never sets it here; osmocom sets it above 2.43 MS/s */
            EXPECT_EQ_U(s.reg0a & 0x10u, 0u);
            EXPECT_EQ_U(s.reg0a & 0x10u, kOldenburger0a[i] & 0x10u);
            if (i < 8) {
                /* 300-1500 kHz: Oldenburger's rows unchanged */
                EXPECT_EQ_U(s.reg0b, kOldenburger0b[i]);
                EXPECT_EQ_U(s.if_hz, kOldenburgerIfKhz[i] * 1000u);
            }
            if (g_failed != failed_before) {
                std::printf("  ^ driver IF table at %u kHz (row %u)\n", static_cast<unsigned>(khz),
                            static_cast<unsigned>(kOldenburgerBwKhz[i]));
            }
        }
    }
    /* 1800-5000 kHz: Oldenburger's reg 0x0b kept for 1800/2200, IF changed; 3000/5000 the reverse */
    R820T2IfSetting s = rtl_r820t2_if_for_rate(1800000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, kOldenburger0b[8]);
    EXPECT_EQ_U(s.if_hz, 1600000u);
    EXPECT_TRUE(s.if_hz != kOldenburgerIfKhz[8] * 1000u);
    s = rtl_r820t2_if_for_rate(2200000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, kOldenburger0b[9]);
    EXPECT_EQ_U(s.if_hz, 1750000u);
    EXPECT_TRUE(s.if_hz != kOldenburgerIfKhz[9] * 1000u);
    s = rtl_r820t2_if_for_rate(3000000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, 0x6fu);
    EXPECT_TRUE(s.reg0b != kOldenburger0b[10]);
    EXPECT_EQ_U(s.if_hz, kOldenburgerIfKhz[10] * 1000u);
    s = rtl_r820t2_if_for_rate(5000000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, 0x6fu);
    EXPECT_TRUE(s.reg0b != kOldenburger0b[11]);
    EXPECT_EQ_U(s.if_hz, kOldenburgerIfKhz[11] * 1000u);
    /* the rates esphome-rtl433 uses, and where they differ from librtlsdr (the A/B) */
    s = rtl_r820t2_if_for_rate(250000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, 0xe8u); EXPECT_EQ_U(s.if_hz, 1700000u);
    s = rtl_r820t2_if_for_rate(1024000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, 0xecu); EXPECT_EQ_U(s.if_hz, 1400000u);
    s = rtl_r820t2_if_for_rate(2048000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0b, 0x8fu); EXPECT_EQ_U(s.if_hz, 1750000u);
    s = rtl_r820t2_if_for_rate(2560000u, R820T2IfMode::Table);
    EXPECT_EQ_U(s.reg0a & 0x10u, 0u); EXPECT_EQ_U(s.reg0b, 0x6fu); EXPECT_EQ_U(s.if_hz, 2000000u);
    for (uint32_t rate : {250000u, 1024000u, 2048000u, 2560000u}) {
        const R820T2IfSetting t = rtl_r820t2_if_for_rate(rate, R820T2IfMode::Table);
        const R820T2IfSetting l = rtl_r820t2_if_for_rate(rate, R820T2IfMode::Librtlsdr);
        EXPECT_TRUE(t.reg0b != l.reg0b && t.if_hz != l.if_hz);
    }
    /* above 5 MS/s both use osmocom's fixed 6/7/8 MHz settings */
    for (uint32_t rate : {5001000u, 6000000u, 6001000u, 7000000u, 7001000u, 8000000u}) {
        const R820T2IfSetting t = rtl_r820t2_if_for_rate(rate, R820T2IfMode::Table);
        const R820T2IfSetting l = rtl_r820t2_if_for_rate(rate, R820T2IfMode::Librtlsdr);
        EXPECT_EQ_U(t.reg0a, l.reg0a); EXPECT_EQ_U(t.reg0b, l.reg0b); EXPECT_EQ_U(t.if_hz, l.if_hz);
    }
    /* demod IF words for the table's IFs at those rates (docs/KCONFIG.md), and the init table's
     * 3.57 MHz bytes 38 11 12 */
    EXPECT_EQ_U(rtl_demod_if_word(1700000u, 28800000u), 0x3c38e4u);
    EXPECT_EQ_U(rtl_demod_if_word(1400000u, 28800000u), 0x3ce38fu);
    EXPECT_EQ_U(rtl_demod_if_word(1750000u, 28800000u), 0x3c1c72u);
    EXPECT_EQ_U(rtl_demod_if_word(2000000u, 28800000u), 0x3b8e39u);
    EXPECT_EQ_U(rtl_demod_if_word(3570000u, 28800000u), 0x381112u);
}

static void test_r820t2_rate_plan(void)
{
    /* host builds define no CONFIG_ symbol: A (vendor fixed) is the default and adds no plan */
    EXPECT_TRUE(kR820T2IfModeBuild == R820T2IfMode::VendorFixed);
    MeasuredTunerBandwidthPlan p{};
    for (RtlProfileId prof : {RtlProfileId::BlogV3, RtlProfileId::NooelecSmartV5}) {
        for (uint32_t rate : kVendorAcceptedRates) {
            EXPECT_TRUE(!r820t2_rate_plan(prof, rate, 915000000u, &p));
            EXPECT_TRUE(!r820t2_rate_plan(prof, rate, 915000000u, &p, R820T2IfMode::VendorFixed));
        }
    }
    /* B and C only on the V3 / Nooelec, never the V4 / V4L or an unknown stick */
    for (R820T2IfMode m : {R820T2IfMode::Table, R820T2IfMode::Librtlsdr}) {
        for (RtlProfileId prof : {RtlProfileId::BlogV4, RtlProfileId::BlogV4L, RtlProfileId::Unknown}) {
            EXPECT_TRUE(!r820t2_rate_plan(prof, 2048000u, 915000000u, &p, m));
        }
        /* nor on the direct-sampling route (tuner bypassed below 24 MHz) */
        EXPECT_TRUE(!r820t2_rate_plan(RtlProfileId::NooelecSmartV5, 2048000u, 7000000u, &p, m));
        EXPECT_TRUE(!r820t2_rate_plan(RtlProfileId::BlogV3, 2048000u, 23999999u, &p, m));
        EXPECT_TRUE(r820t2_rate_plan(RtlProfileId::BlogV3, 2048000u, 24000000u, &p, m));
        EXPECT_TRUE(!r820t2_rate_plan(RtlProfileId::BlogV3, 2048000u, 915000000u, nullptr, m));
    }
    /* C on the Nooelec at the esphome-rtl433 rates: the Nooelec's captured c3 base (its explicit
     * bandwidth plans), 0b and demod IF bytes as the vendor DLL writes them */
    struct Want { uint32_t rate; uint8_t r0a, r0b; uint32_t if_hz; uint8_t b19, b1a, b1b; };
    for (const Want &w : {Want{250000u, 0xc3, 0xe6, 2125000u, 0x3b, 0x47, 0x1d},
                          Want{2048000u, 0xc3, 0xaf, 1625000u, 0x3c, 0x63, 0x8f},
                          Want{2400000u, 0xc3, 0x8f, 1815000u, 0x3b, 0xf7, 0x78},
                          Want{2560000u, 0xd3, 0x6b, 3570000u, 0x38, 0x11, 0x12}}) {
        EXPECT_TRUE(r820t2_rate_plan(RtlProfileId::NooelecSmartV5, w.rate, 433920000u, &p,
                                     R820T2IfMode::Librtlsdr));
        EXPECT_EQ_U(p.reg0a, w.r0a); EXPECT_EQ_U(p.reg0b, w.r0b); EXPECT_EQ_U(p.if_hz, w.if_hz);
        EXPECT_EQ_U(p.if19, w.b19); EXPECT_EQ_U(p.if1a, w.b1a); EXPECT_EQ_U(p.if1b, w.b1b);
        EXPECT_EQ_U(p.requested_hz, 0u);
    }
    /* the Nooelec's own 2.4 MS/s AUTO capture is c3/8f, 1.815 MHz: C reproduces it exactly */
    MeasuredTunerBandwidthPlan autoplan{};
    EXPECT_TRUE(measured_tuner_bandwidth_plan(RtlProfileId::NooelecSmartV5, 96100000u, 0u, &autoplan));
    EXPECT_TRUE(r820t2_rate_plan(RtlProfileId::NooelecSmartV5, 2400000u, 96100000u, &p,
                                 R820T2IfMode::Librtlsdr));
    EXPECT_EQ_U(p.reg0a, autoplan.reg0a); EXPECT_EQ_U(p.reg0b, autoplan.reg0b);
    EXPECT_EQ_U(p.if19, autoplan.if19); EXPECT_EQ_U(p.if1a, autoplan.if1a);
    EXPECT_EQ_U(p.if1b, autoplan.if1b);
    EXPECT_TRUE(p.if_hz - autoplan.if_hz < 100u); /* plan table rounds 1.815 MHz to 1814972 Hz */
    /* B on the V3 (c5 base): 250k e8 / 1.70 MHz, 2048k 8f / 1.75 MHz; never bit 4 at these rates */
    for (const Want &w : {Want{250000u, 0xc5, 0xe8, 1700000u, 0x3c, 0x38, 0xe4},
                          Want{2048000u, 0xc5, 0x8f, 1750000u, 0x3c, 0x1c, 0x72},
                          Want{2560000u, 0xc5, 0x6f, 2000000u, 0x3b, 0x8e, 0x39}}) {
        EXPECT_TRUE(r820t2_rate_plan(RtlProfileId::BlogV3, w.rate, 433920000u, &p,
                                     R820T2IfMode::Table));
        EXPECT_EQ_U(p.reg0a, w.r0a); EXPECT_EQ_U(p.reg0b, w.r0b); EXPECT_EQ_U(p.if_hz, w.if_hz);
        EXPECT_EQ_U(p.if19, w.b19); EXPECT_EQ_U(p.if1a, w.b1a); EXPECT_EQ_U(p.if1b, w.b1b);
    }
    EXPECT_TRUE(std::string_view(rtl_r820t2_if_mode_name(R820T2IfMode::VendorFixed)) == "vendor_fixed");
}

int main(void)
{
    test_r820t2_if_librtlsdr();
    test_r820t2_if_table();
    test_r820t2_if_vendor_captures();
    test_r820t2_rate_plan();
    std::printf("RESULT r820t2_if_mode passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
