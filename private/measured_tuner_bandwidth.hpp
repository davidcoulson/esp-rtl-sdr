#pragma once

#include "rtl_profile.hpp"

#include <cstddef>
#include <cstdint>

/* Vendor-driver control choices at 2.4 MS/s, not measured analog passbands. */
struct MeasuredTunerBandwidthPlan {
    uint32_t requested_hz;
    uint32_t if_hz;
    uint8_t reg0a;
    uint8_t reg0b;
    uint8_t if19;
    uint8_t if1a;
    uint8_t if1b;
};

constexpr uint32_t kMeasuredNativeBandwidths[] = {
    0u, 200000u, 300000u, 500000u, 1000000u, 1800000u, 2400000u,
};
constexpr uint32_t kMeasuredHfBandwidths[] = {0u, 200000u, 500000u, 2400000u};

/* The widths a profile offers at this RF, and how many there are. The count and the array are
 * chosen by ONE rule so they cannot disagree: on the HF upconverter route (V4/V4L at or below
 * 28.8 MHz and not on the direct route) the short HF list applies, otherwise the native list. */
struct MeasuredBandwidthList {
    const uint32_t *values;
    size_t count;
};

constexpr MeasuredBandwidthList measured_tuner_bandwidth_list(RtlProfileId profile,
                                                              uint32_t rf_hz,
                                                              bool hf_direct = false)
{
    if (profile != RtlProfileId::BlogV4 && profile != RtlProfileId::BlogV4L &&
        profile != RtlProfileId::BlogV3 && profile != RtlProfileId::NooelecSmartV5)
        return {nullptr, 0};
    if (rtl_profile_uses_v3_direct_sampling(profile, rf_hz)) return {nullptr, 0};
    return (profile == RtlProfileId::BlogV4 || profile == RtlProfileId::BlogV4L) &&
           rf_hz <= ESP_RTL_SDR_XTAL_HZ && !hf_direct
        ? MeasuredBandwidthList{kMeasuredHfBandwidths,
                                sizeof(kMeasuredHfBandwidths) / sizeof(uint32_t)}
        : MeasuredBandwidthList{kMeasuredNativeBandwidths,
                                sizeof(kMeasuredNativeBandwidths) / sizeof(uint32_t)};
}

constexpr size_t measured_tuner_bandwidth_count(RtlProfileId profile, uint32_t rf_hz,
                                                bool hf_direct = false)
{
    return measured_tuner_bandwidth_list(profile, rf_hz, hf_direct).count;
}

inline bool measured_tuner_bandwidth_plan(RtlProfileId profile, uint32_t rf_hz,
                                           uint32_t width_hz,
                                           MeasuredTunerBandwidthPlan *out,
                                           bool hf_direct = false)
{
    const MeasuredBandwidthList list = measured_tuner_bandwidth_list(profile, rf_hz, hf_direct);
    if (out == nullptr || list.count == 0) return false;
    const uint32_t *widths = list.values;
    const size_t count = list.count;
    bool found = false;
    for (size_t i = 0; i < count; ++i) found |= widths[i] == width_hz;
    if (!found) return false;
    *out = {width_hz, 1814972u,
            static_cast<uint8_t>(profile == RtlProfileId::BlogV4L ? 0xc4 :
                                profile == RtlProfileId::NooelecSmartV5 ? 0xc3 : 0xc5),
            0x8f, 0x3b, 0xf7, 0x78};
    switch (width_hz) {
    case 200000u:
    case 300000u:
        out->if_hz = 2125000u;
        out->reg0b = 0xe6;
        out->if1a = 0x47; out->if1b = 0x1d;
        break;
    case 500000u:
        out->if_hz = 2025000u;
        out->reg0b = 0xe8;
        out->if1a = 0x80; out->if1b = 0x00;
        break;
    case 1000000u:
        out->if_hz = 1700000u;
        out->reg0b = 0xeb;
        out->if19 = 0x3c; out->if1a = 0x38; out->if1b = 0xe4;
        break;
    case 1800000u:
        out->if_hz = 1750000u;
        out->reg0b = 0xac;
        out->if19 = 0x3c; out->if1a = 0x1c; out->if1b = 0x72;
        break;
    default: break;
    }
    if (profile == RtlProfileId::BlogV3 && width_hz == 0) {
        /* Older V3c-specific AUTO policy (2026-09-28): restore the boot IF and
         * filters. Nooelec's 2026-09-30 AUTO capture instead uses c3/8f, 1.815 MHz. */
        out->if_hz = kBlogV3DemodIfHz;
        out->reg0a = kBlogV3BootReg0a; out->reg0b = kBlogV3BootReg0b;
        out->if19 = 0x38; out->if1a = 0x11; out->if1b = 0x12;
    }
    return true;
}

/* Demod IF records of a bandwidth transaction: each IF byte write (0x19, 0x1a,
 * 0x1b) is followed by the page-0x0a reg-0x01 read that the captured init IF
 * sequence and PC live-bandwidth captures place after every demod write.
 * Without the reads, a live transition on the M5 Tab5 left the RTL2832 using
 * the new 0x19 byte with the previous transaction's 0x1a/0x1b (V3c: about
 * +/-95 kHz, V4L: -312 to +386 kHz) while the driver reported the requested RF.
 * The RTL2832 mechanism behind the read is not established. */
constexpr size_t kMeasuredBandwidthDemodIfRecordCount = 6;

inline void measured_bandwidth_demod_if_records(
    const MeasuredTunerBandwidthPlan &plan,
    RtlControlRecord (&out)[kMeasuredBandwidthDemodIfRecordCount])
{
    const RtlControlRecord settle_read = {0x0120, 0x000a, 0xc0, 1, {0, 0, 0, 0, 0, 0, 0, 0}};
    out[0] = {0x1920, 0x0011, 0x40, 1, {plan.if19, 0, 0, 0, 0, 0, 0, 0}};
    out[1] = settle_read;
    out[2] = {0x1a20, 0x0011, 0x40, 1, {plan.if1a, 0, 0, 0, 0, 0, 0, 0}};
    out[3] = settle_read;
    out[4] = {0x1b20, 0x0011, 0x40, 1, {plan.if1b, 0, 0, 0, 0, 0, 0, 0}};
    out[5] = settle_read;
}

/* R820T2 IF filter and IF for a sample rate, as librtlsdr's r82xx_set_bandwidth() picks them when
 * the tuner bandwidth is left automatic (bandwidth = sample rate). Not a vendor capture: this is
 * librtlsdr's table. librtlsdr writes reg 0x0a under mask 0x10 and reg 0x0b under mask 0xef; the
 * RTL2832 demod IF and the PLL LO (RF + IF) follow if_hz. */
struct R820T2IfSetting {
    uint8_t reg0a;
    uint8_t reg0b;
    uint32_t if_hz;
};

inline R820T2IfSetting rtl_r820t2_if_for_rate(uint32_t sample_rate_sps)
{
    constexpr uint32_t kBwKhz[] = {300, 450, 600, 900, 1100, 1200, 1300, 1500, 1800, 2200, 3000, 5000};
    constexpr uint8_t kReg0b[] = {0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xaf, 0x8f, 0x6f, 0x6f};
    constexpr uint32_t kIfKhz[] = {1700, 1650, 1600, 1500, 1400, 1350, 1320, 1270, 1600, 1750, 2000, 3570};
    constexpr size_t kCount = sizeof(kBwKhz) / sizeof(kBwKhz[0]);
    const uint32_t bw_khz = sample_rate_sps / 1000u;
    if (bw_khz > 7000u) return {0x10, 0x0b, 4570000u};
    if (bw_khz > 6000u) return {0x10, 0x2a, 4570000u};
    if (bw_khz > 5000u) return {0x10, 0x6b, 3570000u};
    size_t i = 0;
    while (i + 1 < kCount && bw_khz > kBwKhz[i]) ++i;
    return {static_cast<uint8_t>(i + 1 == kCount ? 0x00 : 0x0f), kReg0b[i], kIfKhz[i] * 1000u};
}

/* RTL2832 demod IF word (page 1 regs 0x19..0x1b), librtlsdr's rtlsdr_set_if_freq() arithmetic.
 * Reproduces the captured 3.57, 2.125, 2.025, 1.815, 1.75 and 1.7 MHz bytes (host-tested). */
inline uint32_t rtl_demod_if_word(uint32_t if_hz, uint32_t xtal_hz = ESP_RTL_SDR_XTAL_HZ)
{
    const int64_t v = (static_cast<int64_t>(if_hz) << 22) / static_cast<int64_t>(xtal_hz);
    return static_cast<uint32_t>(-v) & 0x3fffffu;
}

/* Nooelec cold-reinit filter registers (the mapped reinit slice ends at d3/6b; host-tested). */
constexpr uint8_t kNooelecColdReg0a = 0xd3;
constexpr uint8_t kNooelecColdReg0b = 0x6b;

/* Nooelec SMArt v5 only: the filter, PLL IF and demod IF for a sample rate with no captured plan.
 * The vendor captures cover 2.4 MS/s only, so every other rate otherwise runs the cold 6 MHz-class
 * filter (d3/6b) with the 3.57 MHz IF. Returns false, leaving the captured behaviour in charge,
 * when: the profile is not Nooelec, the rate is 2.4 MS/s, the RF is on the Q route (tuner
 * bypassed), or an explicit tuner bandwidth is applied or pending. requested_hz is 0 (AUTO).
 * The filter bytes are librtlsdr's masked writes applied to the cold d3/6b. */
inline bool rtl_rate_if_plan(RtlProfileId profile, uint32_t sample_rate_sps, uint32_t rf_hz,
                             bool explicit_bandwidth, MeasuredTunerBandwidthPlan *out)
{
    if (out == nullptr || profile != RtlProfileId::NooelecSmartV5 || sample_rate_sps == 0 ||
        sample_rate_sps == ESP_RTL_SDR_RATE_2400K || explicit_bandwidth ||
        rtl_profile_uses_v3_direct_sampling(profile, rf_hz))
        return false;
    const R820T2IfSetting st = rtl_r820t2_if_for_rate(sample_rate_sps);
    const uint32_t word = rtl_demod_if_word(st.if_hz);
    *out = {0u, st.if_hz,
            static_cast<uint8_t>((kNooelecColdReg0a & ~0x10u) | (st.reg0a & 0x10u)),
            static_cast<uint8_t>((kNooelecColdReg0b & ~0xefu) | (st.reg0b & 0xefu)),
            static_cast<uint8_t>((word >> 16) & 0x3fu), static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)};
    return true;
}

enum class RtlBandwidthCommitResult : uint8_t { Applied, RolledBack, Fault };

template <typename Writer>
RtlBandwidthCommitResult rtl_bandwidth_commit(const MeasuredTunerBandwidthPlan &previous,
                                                const MeasuredTunerBandwidthPlan &next,
                                                Writer writer)
{
    if (writer(next, true) == 0) return RtlBandwidthCommitResult::Applied;
    return writer(previous, false) == 0 ? RtlBandwidthCommitResult::RolledBack
                                 : RtlBandwidthCommitResult::Fault;
}
