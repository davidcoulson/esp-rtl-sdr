#pragma once

#include <cstdint>

/**
 * True only when *this caller* is inside the handle's event callback.
 * A handle-wide depth counter is not enough: the delivery task can be in
 * emit() while an application task calls set_tuner_gain_mode().
 */
inline bool esp_rtl_sdr_caller_is_event_callback(uint32_t callback_depth,
                                                 const void *callback_task,
                                                 const void *self_task)
{
    return callback_depth > 0 && callback_task != nullptr && self_task != nullptr &&
           callback_task == self_task;
}

/**
 * Which tasks are inside an event callback right now, and how deeply.
 *
 * One handle-wide {callback_task, depth} pair could not describe two tasks emitting at once
 * (EVT_HEALTH on the delivery task while EVT_RETUNED runs on an app task): the second emitter
 * overwrote callback_task, and the first one's exit cleared it while the second was still
 * inside. That gave a false ERR_REENTRANT to a task that was not in a callback, and let a task
 * that was inside one run a lifecycle call (or apply a retune synchronously instead of
 * deferring it). Here each emitting task claims its own slot with a compare-and-swap and only
 * that task touches the slot's depth, so tasks never interfere. Lock-free; usable from tasks
 * (not ISRs).
 */
struct EspRtlSdrCallbackTracker {
    static constexpr int kSlots = 6;
    const void *task[kSlots] = {};
    uint32_t depth[kSlots] = {};
};

/** Enter a callback on self. @return slot to pass to _exit, or -1 if every slot is taken. */
inline int esp_rtl_sdr_cb_enter(EspRtlSdrCallbackTracker *t, const void *self)
{
    if (t == nullptr || self == nullptr) {
        return -1;
    }
    for (int i = 0; i < EspRtlSdrCallbackTracker::kSlots; ++i) {
        if (__atomic_load_n(&t->task[i], __ATOMIC_ACQUIRE) == self) {
            t->depth[i]++; /* nested emit on the same task */
            return i;
        }
    }
    for (int i = 0; i < EspRtlSdrCallbackTracker::kSlots; ++i) {
        const void *expected = nullptr;
        if (__atomic_compare_exchange_n(&t->task[i], &expected, self, false, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            t->depth[i] = 1;
            return i;
        }
    }
    return -1;
}

inline void esp_rtl_sdr_cb_exit(EspRtlSdrCallbackTracker *t, int slot)
{
    if (t == nullptr || slot < 0 || slot >= EspRtlSdrCallbackTracker::kSlots) {
        return;
    }
    if (t->depth[slot] > 1) {
        t->depth[slot]--;
        return;
    }
    t->depth[slot] = 0;
    __atomic_store_n(&t->task[slot], static_cast<const void *>(nullptr), __ATOMIC_RELEASE);
}

/** True only when self is inside an event callback of this handle. */
inline bool esp_rtl_sdr_cb_is_inside(const EspRtlSdrCallbackTracker *t, const void *self)
{
    if (t == nullptr || self == nullptr) {
        return false;
    }
    for (int i = 0; i < EspRtlSdrCallbackTracker::kSlots; ++i) {
        if (__atomic_load_n(&t->task[i], __ATOMIC_ACQUIRE) == self) {
            return true;
        }
    }
    return false;
}
