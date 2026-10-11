/*
 * Behavioral host tests for multi-dongle identity / caps / frequency policy.
 * No USB / FreeRTOS. Pure profile + V4 frontend composition + tuner isolation.
 */
#include "esp_rtl_sdr.h"
#include "gain_r820t2.hpp"
#include "measured_gain_bias_v4.hpp"
#include "rtl_profile.hpp"
#include "transfers_blog_v3.hpp"
#include "transfers_blog_v4.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>

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

static void test_detection_matrix(void)
{
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V4"),
                (unsigned)RtlProfileId::BlogV4);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V3"),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog",
                                                       "RTL-SDR Blog V3"),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "Nooelec",
                                                       "NESDR SMArt v5"),
                (unsigned)RtlProfileId::NooelecSmartV5);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "Nooelec",
                                                       "NESDR SMArt v5 Bulk"),
                (unsigned)RtlProfileId::NooelecSmartV5);

    /* Never treat bare / unknown 0bda:2838 as V4. */
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTL2832U", "Generic"),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog", "unknown"),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic", {}),
                (unsigned)RtlProfileId::Unknown);
}

static void test_unknown_reject_and_v3_probe(void)
{
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {true, 0x96}),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {true, 0x69}),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {false, 0x96}),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {true, 0x00}),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x1234, 0x2838, "RTLSDRBlog", "Blog V4", {}),
                (unsigned)RtlProfileId::Unknown);

    /* Descriptor always wins over probe. */
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V4",
                                             {true, 0x96}),
                (unsigned)RtlProfileId::BlogV4);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "Nooelec", "NESDR SMArt v5",
                                             {true, 0x96}),
                (unsigned)RtlProfileId::NooelecSmartV5);

    EXPECT_EQ_U(kBlogV3ProbeSelect.value, 0x0034);
    EXPECT_EQ_U(kBlogV3ProbeRead.value, 0x0034);
    EXPECT_EQ_U(kBlogV3ProbeSelect.index, 0x0610);
    EXPECT_EQ_U(kBlogV3ProbeRead.index, 0x0600);
    EXPECT_EQ_U(kBlogV3ProbeSelect.request_type, 0x40);
    EXPECT_EQ_U(kBlogV3ProbeRead.request_type, 0xc0);
}

static void test_tuner_isolation(void)
{
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::BlogV4), 0x0074);
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::BlogV3), 0x0034);
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::NooelecSmartV5), 0x0034);
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::Unknown), 0);

    /* Remap policy: only low byte of tuner IR value when index is 0x0610/0x0600. */
    RtlControlRecord v4_ir = {0x0074, 0x0610, 0x40, 2, {0x05, 0xa3}};
    EXPECT_EQ_U(v4_ir.value & 0xff, kBlogV4TunerI2cValue);
    EXPECT_TRUE(kRtlFinalTuneTemplate[0].value == 0x0074);

    size_t v4_allowed = 0;
    size_t v3_allowed = 0;
    size_t nooelec_allowed = 0;
    for (const auto &record : kRtlInitTransfers) {
        v4_allowed += rtl_profile_allows_init_record(RtlProfileId::BlogV4, record) ? 1 : 0;
        v3_allowed += rtl_profile_allows_init_record(RtlProfileId::BlogV3, record) ? 1 : 0;
        nooelec_allowed +=
            rtl_profile_allows_init_record(RtlProfileId::NooelecSmartV5, record) ? 1 : 0;
        if (record.value == 0x3001 || record.value == 0x3003 || record.value == 0x3004) {
            EXPECT_TRUE(rtl_profile_allows_init_record(RtlProfileId::BlogV4, record));
            EXPECT_TRUE(!rtl_profile_allows_init_record(RtlProfileId::BlogV3, record));
            EXPECT_TRUE(!rtl_profile_allows_init_record(RtlProfileId::NooelecSmartV5, record));
        }
    }
    EXPECT_EQ_U(v4_allowed, std::size(kRtlInitTransfers));
    EXPECT_TRUE(v3_allowed < v4_allowed);
    EXPECT_EQ_U(v3_allowed, nooelec_allowed);
}

static void test_frequency_policy(void)
{
    /* V4: experimental LF/HF upconverter path, exact requested RF retained. */
    const uint32_t v4_rf_hz[] = {24000u, 60000u, 135600u, 147300u, 474000u,
                                 500000u, 1000000u, 10000000u, 28799999u,
                                 28800000u, 28800001u, 28801000u, 96100000u, 162400000u,
                                 433000000u, 1090000000u};
    for (const uint32_t rf_hz : v4_rf_hz) {
        EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV4, rf_hz));
        const uint32_t expected = rf_hz < 28800000u ? rf_hz + 28800000u : rf_hz;
        EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV4, rf_hz), expected);
    }
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::BlogV4, 23999u));
    EXPECT_TRUE(rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV4));

    /* Nooelec: reject < 24 MHz; no V4 HF LO offset / routing. */
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 10000000u));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 23999999u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 24000000u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 100000000u));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::NooelecSmartV5, 100000000u),
                100000000u);
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::NooelecSmartV5));

    /* V3: captured Q-branch direct sampling below the 24 MHz tuner floor. */
    const uint32_t direct_rf_hz[] = {60000u, 135600u, 147300u, 474000u, 500000u,
                                     1000000u, 10000000u, 20000000u, 23999999u};
    for (const uint32_t rf_hz : direct_rf_hz) {
        EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, rf_hz));
        EXPECT_TRUE(rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV3, rf_hz));
        EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, rf_hz), 0u);
    }
    const uint32_t normal_v3_rf_hz[] = {24000000u, 28800000u, 96100000u,
                                        162400000u, 433000000u, 1090000000u};
    for (const uint32_t rf_hz : normal_v3_rf_hz) {
        EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, rf_hz));
        EXPECT_TRUE(!rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV3, rf_hz));
        EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, rf_hz), rf_hz);
    }
    EXPECT_TRUE(rtl_profile_supports_stream(RtlProfileId::BlogV3));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, 100000000u),
                100000000u);
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV3));
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV3));
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::NooelecSmartV5));
    EXPECT_TRUE(!rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV4));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::Unknown, 100000000u));

    struct DirectNcoCase { uint32_t rf_hz; uint32_t nco; };
    constexpr DirectNcoCase captured[] = {
        {60000u, 0x3fdddeu}, {135600u, 0x3fb2dcu}, {147300u, 0x3fac34u},
        {472500u, 0x3ef334u}, {1000123u, 0x3dc70bu}, {10000000u, 0x29c71du},
        {20000000u, 0x138e39u}, {23999999u, 0x0aaaabu},
    };
    for (const auto &point : captured) {
        EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(point.rf_hz), point.nco);
    }

    EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(147300u, 100), 0x3fac32u);
    EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(147300u, -100), 0x3fac36u);
    EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(23999999u, 1000), 0x0a9d04u);
}

static void test_matched_if_policy(void)
{
    EXPECT_EQ_U((uint32_t)rtl_profile_pll_if_offset_hz(RtlProfileId::BlogV3), 3570000u);
    EXPECT_EQ_U(rtl_profile_demod_if_restore_hz(RtlProfileId::BlogV3), 3570000u);
    EXPECT_EQ_U((uint32_t)rtl_profile_pll_if_offset_hz(RtlProfileId::BlogV4), 1814972u);
    EXPECT_EQ_U(rtl_profile_demod_if_restore_hz(RtlProfileId::BlogV4), 0u);
    /* Same R820T2 tuner as BlogV3: its measured 3.57 MHz IF, verified receiving on a SMArt v5 */
    EXPECT_EQ_U((uint32_t)rtl_profile_pll_if_offset_hz(RtlProfileId::NooelecSmartV5), 3570000u);
    EXPECT_EQ_U(rtl_profile_demod_if_restore_hz(RtlProfileId::NooelecSmartV5), 3570000u);

    const uint16_t values[] = {0x1920, 0x0120, 0x1a20, 0x0120, 0x1b20, 0x0120};
    const uint16_t indices[] = {0x0011, 0x000a, 0x0011, 0x000a, 0x0011, 0x000a};
    const uint8_t request_types[] = {0x40, 0xc0, 0x40, 0xc0, 0x40, 0xc0};
    const uint8_t data[] = {0x38, 0x00, 0x11, 0x00, 0x12, 0x00};
    EXPECT_EQ_U(kRtlStandardIfLast - kRtlStandardIfFirst + 1, 6u);
    for (size_t i = 0; i < 6; ++i) {
        const auto &record = kRtlInitTransfers[kRtlStandardIfFirst + i];
        EXPECT_EQ_U(record.value, values[i]);
        EXPECT_EQ_U(record.index, indices[i]);
        EXPECT_EQ_U(record.request_type, request_types[i]);
        EXPECT_EQ_U(record.length, 1u);
        EXPECT_EQ_U(record.data[0], data[i]);
    }
}

static void test_v3_direct_transition_records(void)
{
    EXPECT_EQ_U(std::size(kBlogV3DirectEnable), 8u);
    EXPECT_EQ_U(kBlogV3DirectEnable[0].value, 0xb120u);
    EXPECT_EQ_U(kBlogV3DirectEnable[0].data[0], 0x1au);
    EXPECT_EQ_U(kBlogV3DirectEnable[2].value, 0x1520u);
    EXPECT_EQ_U(kBlogV3DirectEnable[2].data[0], 0x00u);
    EXPECT_EQ_U(kBlogV3DirectEnable[4].data[0], 0x4du);
    EXPECT_EQ_U(kBlogV3DirectEnable[6].data[0], 0x90u);
    EXPECT_EQ_U(std::size(kBlogV3DirectDisable), 4u);
    EXPECT_EQ_U(kBlogV3DirectDisable[0].data[0], 0x01u);
    EXPECT_EQ_U(kBlogV3DirectDisable[2].data[0], 0x80u);
    EXPECT_EQ_U(kRtlTunerCleanupLast, 13u);
    EXPECT_EQ_U(kRtlTunerReinitFirst, 363u);
    EXPECT_EQ_U(kRtlTunerReinitLast, 419u);
    EXPECT_EQ_U(kRtlInitTransfers[kRtlTunerReinitFirst].value, 0x0074u);
    EXPECT_EQ_U(kRtlInitTransfers[kRtlTunerReinitLast].value, 0x0120u);
    EXPECT_EQ_U(std::size(kBlogV3TunerRepeaterOn), 2u);
    EXPECT_EQ_U(kBlogV3TunerRepeaterOn[0].value, 0x0120u);
    EXPECT_EQ_U(kBlogV3TunerRepeaterOn[0].data[0], 0x18u);

    EXPECT_TRUE(rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV3, 24000000u));
    EXPECT_TRUE(rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV3, 99100000u));
    EXPECT_TRUE(!rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV3, 23999999u));
    EXPECT_TRUE(!rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV4, 99100000u));
    EXPECT_TRUE(!rtl_profile_needs_cold_tuner_reinit(RtlProfileId::NooelecSmartV5,
                                                     99100000u));

    uint8_t full_tail_reg05 = 0;
    uint8_t reinit_reg05 = 0;
    for (size_t i = 0; i < std::size(kRtlInitTransfers); ++i) {
        const auto &record = kRtlInitTransfers[i];
        if (record.value == 0x0074u && record.index == 0x0610u &&
            record.length >= 2 && record.data[0] == 0x05u) {
            full_tail_reg05 = record.data[1];
            if (i >= kRtlTunerReinitFirst && i <= kRtlTunerReinitLast) {
                reinit_reg05 = record.data[1];
            }
        }
    }
    EXPECT_EQ_U(full_tail_reg05, 0xe3u);
    EXPECT_EQ_U(reinit_reg05, 0x83u);

    const uint32_t retunes[] = {96100000u, 10000000u, 147300u, 96100000u, 147300u};
    const bool direct[] = {false, true, true, false, true};
    for (size_t i = 0; i < std::size(retunes); ++i) {
        EXPECT_EQ_U(rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV3, retunes[i]),
                    direct[i]);
    }
}

static void test_capability_matrix(void)
{
    const uint32_t v4 = rtl_profile_device_capabilities(RtlProfileId::BlogV4);
    const uint32_t v3 = rtl_profile_device_capabilities(RtlProfileId::BlogV3);
    const uint32_t noe = rtl_profile_device_capabilities(RtlProfileId::NooelecSmartV5);
    const uint32_t unk = rtl_profile_device_capabilities(RtlProfileId::Unknown);

    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_HF_UPCONVERTER) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_GAIN) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_BIAS_TEE) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) == 0);

    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_RETUNE) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_GAIN) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_GAIN_AUTO) != 0); /* librtlsdr's R82xx auto gain recipe */
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_BIAS_TEE) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) != 0);
    EXPECT_EQ_U(rtl_profile_default_gain_mode(RtlProfileId::BlogV4),
                ESP_RTL_SDR_GAIN_MODE_AUTO);
    EXPECT_EQ_U(rtl_profile_default_gain_mode(RtlProfileId::BlogV3),
                ESP_RTL_SDR_GAIN_MODE_AUTO);
    EXPECT_EQ_U(rtl_profile_default_gain_mode(RtlProfileId::NooelecSmartV5),
                ESP_RTL_SDR_GAIN_MODE_AUTO);

    constexpr uint8_t expected_r820t2_stages[][2] = {
        {0x90, 0x60}, {0x91, 0x60}, {0x91, 0x61}, {0x92, 0x61},
        {0x92, 0x62}, {0x93, 0x62}, {0x93, 0x63}, {0x94, 0x63},
        {0x94, 0x64}, {0x95, 0x64}, {0x95, 0x65}, {0x96, 0x65},
        {0x96, 0x66}, {0x97, 0x66}, {0x97, 0x67}, {0x98, 0x67},
        {0x98, 0x68}, {0x99, 0x68}, {0x99, 0x69}, {0x9a, 0x69},
        {0x9a, 0x6a}, {0x9b, 0x6a}, {0x9b, 0x6b}, {0x9c, 0x6b},
        {0x9c, 0x6c}, {0x9d, 0x6c}, {0x9d, 0x6d}, {0x9e, 0x6d},
        {0x9f, 0x6e},
    };
    EXPECT_EQ_U(std::size(kR820T2GainSteps),
                std::size(expected_r820t2_stages));
    for (size_t i = 0; i < std::size(expected_r820t2_stages); ++i) {
        EXPECT_EQ_U(kR820T2GainSteps[i].reg05,
                    expected_r820t2_stages[i][0]);
        EXPECT_EQ_U(kR820T2GainSteps[i].reg07,
                    expected_r820t2_stages[i][1]);
    }
    EXPECT_EQ_U(r820t2_nearest_gain_index(std::numeric_limits<int>::min()), 0u);
    EXPECT_EQ_U(r820t2_nearest_gain_index(0), 0u);
    EXPECT_EQ_U(r820t2_nearest_gain_index(10), 1u);
    EXPECT_EQ_U(r820t2_nearest_gain_index(496),
                std::size(kR820T2GainSteps) - 1);
    EXPECT_EQ_U(r820t2_nearest_gain_index(std::numeric_limits<int>::max()),
                std::size(kR820T2GainSteps) - 1);

    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_RETUNE) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_GAIN) != 0); /* shared apply_r820t2_gain_records() path */
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_GAIN_AUTO) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_BIAS_TEE) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) == 0);

    EXPECT_EQ_U(unk, 0);
    EXPECT_EQ_U(esp_rtl_sdr_get_capabilities(), rtl_profile_library_capabilities());
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_BLOG_V4),
                             "blog_v4_r828d") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_BLOG_V3),
                             "blog_v3_r820t2") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5),
                             "nooelec_smart_v5_r820t2") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_UNKNOWN),
                             "unknown") == 0);
}

static void test_profile_transition_matrix(void)
{
    /* Simulated hotplug: attached caps → detach clears to unknown/0. */
    RtlProfileId cur = RtlProfileId::BlogV4;
    uint32_t caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_STREAM) != 0);

    cur = RtlProfileId::Unknown;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_EQ_U(caps, 0);

    cur = RtlProfileId::NooelecSmartV5;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);

    cur = RtlProfileId::Unknown;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_EQ_U(caps, 0);

    cur = RtlProfileId::BlogV3;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);

    cur = RtlProfileId::BlogV4;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_HF_UPCONVERTER) != 0);

    /* V4 frontend composition must remain identical (PR #18 regression gate). */
    const auto hf = measured_v4_frontend_plan(1120000u, false, 0x03);
    const auto vhf = measured_v4_frontend_plan(100000000u, false, 0x03);
    const auto uhf = measured_v4_frontend_plan(1090000000u, false, 0x03);
    EXPECT_EQ_U(hf.reg06, 0x38);
    EXPECT_EQ_U(hf.reg05, 0xa3);
    EXPECT_EQ_U(hf.gpo, 0x18);
    EXPECT_EQ_U(vhf.reg06, 0x30);
    EXPECT_EQ_U(vhf.reg05, 0xe3);
    EXPECT_EQ_U(vhf.gpo, 0x38);
    EXPECT_EQ_U(uhf.reg06, 0x30);
    EXPECT_EQ_U(uhf.reg05, 0x83);
    EXPECT_EQ_U(uhf.gpo, 0x38);
    EXPECT_EQ_U(measured_v4_frontend_plan(28800000u, false, 0x03).reg06, 0x38);
    EXPECT_TRUE(!esp_rtl_sdr_frequency_uses_hf_upconverter(28800000u));
}

/* hardcoreerik/esp-rtl-sdr#25: the R820T2 front end must follow the tuned band */
static void test_r820t2_band_select(void)
{
    /* 433.92 MHz: bypass mux (0x41), tracking filter caps 0 */
    const R820T2BandRow *b433 = rtl_r820t2_band_for_hz(433920000u);
    EXPECT_EQ_U(b433->mhz, 310u);
    EXPECT_EQ_U(b433->rf_mux_ploy, 0x41u);
    EXPECT_EQ_U(b433->tf_c, 0x00u);
    EXPECT_EQ_U(b433->open_d, 0x00u);
    /* the byte actually written for r1a from the capture's 0x2a, as seen live on hardware */
    EXPECT_EQ_U((0x2au & ~0xc3u) | b433->rf_mux_ploy, 0x69u);
    const R820T2BandRow *b915 = rtl_r820t2_band_for_hz(915000000u);
    EXPECT_EQ_U(b915->mhz, 650u);
    EXPECT_EQ_U((0x2au & ~0xc3u) | b915->rf_mux_ploy, 0x68u);
    /* FM band keeps the capture's own values (that's where it was recorded) */
    const R820T2BandRow *bfm = rtl_r820t2_band_for_hz(100000000u);
    EXPECT_EQ_U(bfm->rf_mux_ploy, 0x02u);
    EXPECT_EQ_U(bfm->tf_c, 0x34u);
    /* below 75 MHz the open-drain input is on */
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(30000000u)->open_d, 0x08u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(75000000u)->open_d, 0x00u);
    /* row boundaries: exactly on a row start selects that row */
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(310000000u)->mhz, 310u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(309999999u)->mhz, 280u);
    /* keyed on the LO like librtlsdr: RF 307 MHz + 3.57 MHz IF = LO 310.57 MHz -> 310 row */
    EXPECT_EQ_U(rtl_r820t2_lo_hz(307000000u, 3570000.0), 310570000u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(rtl_r820t2_lo_hz(307000000u, 3570000.0))->mhz, 310u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(rtl_r820t2_lo_hz(433920000u, 3570000.0))->mhz, 310u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(rtl_r820t2_lo_hz(915000000u, 3570000.0))->mhz, 650u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(1700000000u)->mhz, 650u);
    /* table is sorted, or the lookup picks the wrong row */
    for (size_t i = 1; i < std::size(kR820T2Bands); ++i) {
        EXPECT_TRUE(kR820T2Bands[i].mhz > kR820T2Bands[i - 1].mhz);
    }
}

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
    /* PLL: LO = RF + IF (r82xx_set_freq), and the band row follows the LO */
    EXPECT_EQ_U(rtl_r820t2_lo_hz(433920000u, 2125000.0), 436045000u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(rtl_r820t2_lo_hz(433920000u, 2125000.0))->mhz, 310u);
    EXPECT_EQ_U(rtl_r820t2_lo_hz(915000000u, 1625000.0), 916625000u);
    EXPECT_EQ_U(rtl_r820t2_band_for_hz(rtl_r820t2_lo_hz(915000000u, 1625000.0))->mhz, 650u);
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
    /* host builds have no CONFIG_ESP_RTL_SDR_ESP_R820T2_IF_LIBRTLSDR: the table is the default */
    EXPECT_TRUE(kR820T2IfModeBuild == R820T2IfMode::Table);
    /* each row covers rates above the previous row up to and including its own bandwidth */
    for (size_t i = 0; i < std::size(kOldenburgerBwKhz); ++i) {
        const uint32_t lo_khz = i == 0 ? 0u : kOldenburgerBwKhz[i - 1] + 1u;
        for (uint32_t khz : {lo_khz, kOldenburgerBwKhz[i]}) {
            const int failed_before = g_failed;
            const R820T2IfSetting s = rtl_r820t2_if_for_rate(khz * 1000u);
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
    R820T2IfSetting s = rtl_r820t2_if_for_rate(1800000u);
    EXPECT_EQ_U(s.reg0b, kOldenburger0b[8]);
    EXPECT_EQ_U(s.if_hz, 1600000u);
    EXPECT_TRUE(s.if_hz != kOldenburgerIfKhz[8] * 1000u);
    s = rtl_r820t2_if_for_rate(2200000u);
    EXPECT_EQ_U(s.reg0b, kOldenburger0b[9]);
    EXPECT_EQ_U(s.if_hz, 1750000u);
    EXPECT_TRUE(s.if_hz != kOldenburgerIfKhz[9] * 1000u);
    s = rtl_r820t2_if_for_rate(3000000u);
    EXPECT_EQ_U(s.reg0b, 0x6fu);
    EXPECT_TRUE(s.reg0b != kOldenburger0b[10]);
    EXPECT_EQ_U(s.if_hz, kOldenburgerIfKhz[10] * 1000u);
    s = rtl_r820t2_if_for_rate(5000000u);
    EXPECT_EQ_U(s.reg0b, 0x6fu);
    EXPECT_TRUE(s.reg0b != kOldenburger0b[11]);
    EXPECT_EQ_U(s.if_hz, kOldenburgerIfKhz[11] * 1000u);
    /* the rates esphome-rtl433 uses, and where they differ from librtlsdr (the A/B) */
    s = rtl_r820t2_if_for_rate(250000u);
    EXPECT_EQ_U(s.reg0b, 0xe8u); EXPECT_EQ_U(s.if_hz, 1700000u);
    s = rtl_r820t2_if_for_rate(1024000u);
    EXPECT_EQ_U(s.reg0b, 0xecu); EXPECT_EQ_U(s.if_hz, 1400000u);
    s = rtl_r820t2_if_for_rate(2048000u);
    EXPECT_EQ_U(s.reg0b, 0x8fu); EXPECT_EQ_U(s.if_hz, 1750000u);
    s = rtl_r820t2_if_for_rate(2560000u);
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

static void test_r82xx_bitrev(void)
{
    EXPECT_EQ_U(r82xx_bitrev(0x69), 0x96u); /* R820T2 chip id as it arrives over I2C */
    EXPECT_EQ_U(r82xx_bitrev(0x00), 0x00u);
    EXPECT_EQ_U(r82xx_bitrev(0xff), 0xffu);
    EXPECT_EQ_U(r82xx_bitrev(0x01), 0x80u);
    for (unsigned v = 0; v < 256; ++v) {
        EXPECT_EQ_U(r82xx_bitrev(r82xx_bitrev(static_cast<uint8_t>(v))), v);
    }
}

static void test_r820t2_tune_keeps_auto_vga(void)
{
    /* Record 6 of the tune template must be the 0x0c write this helper patches */
    EXPECT_EQ_U(kRtlFinalTuneTemplate[kRtlFinalTuneReg0cIndex].data[0], 0x0cu);
    const uint8_t tmpl = kRtlFinalTuneTemplate[kRtlFinalTuneReg0cIndex].data[1];
    EXPECT_EQ_U(tmpl, 0x68u);
    /* AUTO applied: both R820T2 profiles keep librtlsdr's fixed AUTO VGA */
    EXPECT_EQ_U(rtl_r820t2_tune_reg0c(RtlProfileId::NooelecSmartV5, true, tmpl), 0x6bu);
    EXPECT_EQ_U(rtl_r820t2_tune_reg0c(RtlProfileId::BlogV3, true, tmpl), 0x6bu);
    /* No AUTO (manual or never set): the template's value */
    EXPECT_EQ_U(rtl_r820t2_tune_reg0c(RtlProfileId::NooelecSmartV5, false, tmpl), 0x68u);
    /* Not an R820T2 remap profile: untouched even with AUTO */
    EXPECT_EQ_U(rtl_r820t2_tune_reg0c(RtlProfileId::BlogV4, true, tmpl), 0x68u);
}

int main(void)
{
    test_r820t2_tune_keeps_auto_vga();
    test_r820t2_band_select();
    test_r820t2_if_librtlsdr();
    test_r820t2_if_table();
    test_r820t2_if_vendor_captures();
    test_r82xx_bitrev();
    test_detection_matrix();
    test_unknown_reject_and_v3_probe();
    test_tuner_isolation();
    test_frequency_policy();
    test_matched_if_policy();
    test_v3_direct_transition_records();
    test_capability_matrix();
    test_profile_transition_matrix();
    std::printf("RESULT profiles passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
