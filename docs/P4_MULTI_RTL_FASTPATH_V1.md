# ESP32-P4 multi-RTL READ fast-path experiment v1

Base commit: `018c9d0e561a8e05435a90f149f61de51d192d79`

Purpose: test whether OrcNode's remaining 11.2 -> 14.4 MB/s gap is dominated by software overhead in the READ delivery path rather than the P4 USB-HS bus.

## Changes

- `esp_rtl_sdr_read()` drains the pull ring with at most two `memcpy()` calls instead of one byte/modulo/counter operation per byte.
- READ-only delivery bypasses `IqSlot`, `free_q`, `filled_q`, and the URB -> slot copy after the pull ring exists.
- The USB transfer callback never waits for `pull_mux`; it uses a zero-wait try-lock and immediately resubmits the URB.
- Delivery-task users of `pull_ring_push()` keep the prior bounded 5 ms wait.
- `consumer_drops` is treated as a dropped-buffer/event count in the pull-ring path rather than mixing dropped bytes into the public `dropped_buffers` field.
- READ-only `bytes_total` counts completed USB ingress even when consumer delivery misses.

## Deliberate limits of v1

This is not a zero-copy design. READ-only mode still performs one copy from the completed URB into the pull ring and one copy from the pull ring into the caller buffer.

The URB -> pull-ring copy still occurs inside the USB transfer completion callback. The intent of v1 is to remove avoidable queue/copy/byte-loop overhead first and measure the result before redesigning buffer ownership.

No URB geometry changes are included in this patch.

## Required validation

Test in this order:

1. 1 receiver @ 2.4 MS/s
2. 2 receivers @ 2.4 + 2.4 MS/s
3. 3 receivers @ 2.4 + 2.4 + 1.024 MS/s
4. 3 receivers @ 2.4 + 2.4 + 2.4 MS/s

For each configuration record:

- per-receiver `bytes_received`
- effective sample rate
- `usb_transfer_errors`
- `usb_timeouts`
- `short_transfers`
- `buffer_overruns`
- `dropped_buffers`
- endpoint recoveries
- watchdog events
- HTTP responsiveness

Run the transport-only case first, then repeat with OrcNode scanner/retunes enabled.

If 3 x 2.4 MS/s remains unstable, the next experiment should instrument callback execution time/ring contention before changing URB geometry. A later controlled geometry matrix can compare 4x16 KiB, 2x32 KiB, 3x32 KiB and 4x32 KiB subject to internal DMA memory headroom.

## Important

Do not merge this branch based on code inspection alone. It needs ESP32-P4 hardware validation with all three RTL-SDRs.
