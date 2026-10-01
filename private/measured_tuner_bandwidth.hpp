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

/* Whether the measured bandwidth transaction owns filter, PLL IF and demod IF at this rate and RF.
 * Only at 2.4 MS/s and only where a width list exists. Profiles that restore their own demod IF at
 * start (V3c, Nooelec) use it only once a width has been requested or applied. Stream start, hot
 * retune and live sample-rate change all decide with this one rule. */
inline bool rtl_measured_bandwidth_in_use(RtlProfileId profile, uint32_t sample_rate_sps,
                                          uint32_t rf_hz, bool hf_direct,
                                          bool bandwidth_applied_valid, bool pending_bandwidth)
{
    return sample_rate_sps == ESP_RTL_SDR_RATE_2400K &&
           measured_tuner_bandwidth_count(profile, rf_hz, hf_direct) != 0 &&
           (rtl_profile_demod_if_restore_hz(profile) == 0 || bandwidth_applied_valid ||
            pending_bandwidth);
}

/* Filter, PLL IF and demod IF that stream start leaves when no bandwidth plan is in use, as a plan
 * so the bandwidth transaction can write it. Start runs the shared init table (ends c5/8f and the
 * 1.815 MHz demod IF 3b f7 78), then on R820T2-remap profiles the tuner reinit slice (d5/6b; the
 * Nooelec record mapping makes that d3), then on V3c/Nooelec the standard 3.57 MHz IF (38 11 12).
 * Not defined in direct-Q mode, where the tuner is bypassed. */
inline bool measured_tuner_bandwidth_baseline(RtlProfileId profile, uint32_t rf_hz,
                                              MeasuredTunerBandwidthPlan *out)
{
    if (out == nullptr || measured_tuner_bandwidth_count(profile, rf_hz) == 0) return false;
    *out = {0u, static_cast<uint32_t>(rtl_profile_pll_if_offset_hz(profile)),
            0xc5, 0x8f, 0x3b, 0xf7, 0x78};
    if (rtl_profile_needs_cold_tuner_reinit(profile, rf_hz)) {
        out->reg0a = kBlogV3BootReg0a;
        out->reg0b = kBlogV3BootReg0b;
    }
    if (rtl_profile_demod_if_restore_hz(profile) != 0) {
        out->if19 = 0x38; out->if1a = 0x11; out->if1b = 0x12;
    }
    return true;
}

/* True when two plans program the same filter, PLL IF and demod IF (the requested width aside). */
constexpr bool measured_tuner_bandwidth_same_controls(const MeasuredTunerBandwidthPlan &a,
                                                      const MeasuredTunerBandwidthPlan &b)
{
    return a.if_hz == b.if_hz && a.reg0a == b.reg0a && a.reg0b == b.reg0b && a.if19 == b.if19 &&
           a.if1a == b.if1a && a.if1b == b.if1b;
}

/* What a live sample-rate change does to the tuner-bandwidth state so the stream ends up where a
 * stop/start at the new rate would put it: apply the measured plan (2.4 MS/s, as start and retune
 * do), put back the start baseline (leaving a plan that was in effect at the old rate), or leave
 * filter and IF alone (no plan before or after). */
enum class RtlRateChangeBandwidth : uint8_t { Keep, ApplyPlan, RestoreBaseline };

inline RtlRateChangeBandwidth rtl_rate_change_bandwidth(RtlProfileId profile,
                                                        uint32_t new_rate_sps,
                                                        uint32_t rf_hz, bool hf_direct,
                                                        bool bandwidth_applied_valid,
                                                        bool pending_bandwidth)
{
    if (rtl_measured_bandwidth_in_use(profile, new_rate_sps, rf_hz, hf_direct,
                                      bandwidth_applied_valid, pending_bandwidth)) {
        return RtlRateChangeBandwidth::ApplyPlan;
    }
    return bandwidth_applied_valid &&
                   measured_tuner_bandwidth_count(profile, rf_hz, hf_direct) != 0
               ? RtlRateChangeBandwidth::RestoreBaseline
               : RtlRateChangeBandwidth::Keep;
}

/* A failed live rate change stays pending and the delivery task runs the whole sequence again.
 * After this many failed attempts in a row the request is dropped; the driver then still reports
 * the last rate it applied, although the resampler may already have been rewritten. */
constexpr uint8_t kRtlRateChangeMaxAttempts = 3;

constexpr bool rtl_rate_change_retry(uint8_t failed_attempts)
{
    return failed_attempts < kRtlRateChangeMaxAttempts;
}
