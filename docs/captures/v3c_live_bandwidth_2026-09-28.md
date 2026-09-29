# Live tuner-bandwidth transition: PC capture and Tab5 hardware acceptance — 2026-09-28

Covers the RTL-SDR Blog V3c (the capture subject), V4L and V4.

Method statement: the PC side was driven only through the public API of the RTL-SDR
Blog `rtlsdr.dll` (v4l-driver-1.4.0, x64) as a black-box stimulus, with USBPcap as the
evidence, per `docs/CLEAN_ROOM.md`. No library source was read or copied. Everything
below marked *observed* is taken from a capture, readback or IQ file; *inferred* is
not proven.

## Symptom

On the M5 Tab5 with a V3c at FM 96.1 MHz, selecting a tuner bandwidth in RF Lab
moved the spectrum sideways while the UI and driver still reported 96.1 MHz. A later
+100/-100 kHz retune re-centred it. Returning to AUTO left the wide passband narrow.
The V4L later showed the same class of fault with larger offsets.

## PC capture (observed)

- Device: `Realtek RTL2838UHIDIR`, serial `00000001` (the V3c test unit), WinUSB, USB bus 4 address 8.
- One continuous open session: 2.4 MS/s, 96.1 MHz, manual gain 27, then
  `set_tuner_bandwidth(200000)`, `(0)`, `set_center_freq(96100000)`, +100 kHz, -100 kHz.
  Every call returned 0; the library printed `[R82XX] PLL not locked!` once, on the first
  `set_center_freq` after open.
- Capture `20260928-131711-v3c-live-bw.pcapng`, sha256
  `E4531CDD583E0561BF4B977EDDD6F00451EE21DB9CFC96AB9CE5E6C94736BDB1` (12 MB, held locally,
  not committed). Timed autostop; tshark reported one dropped packet across the four
  USBPcap roots, and all 779 V3c control submits have matching completions.
- Decoded control transfers: `v3c_live_bandwidth_pc_control_transfers_2026-09-28.txt`.

`set_tuner_bandwidth(200000)` on the wire (frames 18825-18941):

    repeater on (demod p1 r01=18), read p0a r01
    tuner 0a=c5, 0b=e6
    demod 19=3b, read p0a r01, 1a=47, read p0a r01, 1b=1d, read p0a r01
    repeater on, read
    full tuner tune: 17=20 1a=2a 1b=34 10=84 08=c0 09=40 0c=68 10=84 1a=22 12=06
                     ptr00 R5, 10=84 14=4a 12=06 16=91 15=c6, ptr00 R3, 1a=2a
    repeater off (p1 r01=10), read

`set_tuner_bandwidth(0)` has the same shape with `0b=8f`, IF `3b f7 78` and PLL `14=4a 16=65 15=b0`.
`set_center_freq` sends repeater-on, the same full tuner tune, and no demod IF writes.

PC spectrum, same centroid method as the Tab5 gate (observed): carrier +0.5 / +1.2 / +1.1 /
+1.0 / +0.9 kHz at baseline / 200 kHz / AUTO / explicit 96.1 / step. The PC stays centred
through every live transition and also loses the left side at 200 kHz, so the lopsided
narrow passband is how the tuner behaves and is not the fault.

## Faults found on the Tab5 (observed)

1. **Demod IF sequencing (all profiles).** `run_bandwidth_program` wrote 0x19/0x1a/0x1b
   consecutively; the PC and the captured init IF sequence read page 0x0a reg 0x01 after every
   demod write.
   - V3c, before the fix: carrier +93.7 kHz at 200 kHz and -89.6 kHz back at AUTO.
   - V4L, before the fix: -312, -4.7, +98.4, -124.1, -49.9, +385.6 and +0.3 kHz through 200k, 300k,
     500k, 1.0M, 1.8M, 2.4M and AUTO.
   - Model (*inferred*; the RTL2832 mechanism is not established): the demodulator ends each
     transaction using the new 0x19 with the previous transaction's 0x1a/0x1b. Effective demod IF
     minus PLL IF then predicts the V4L stage offsets to -310, 0, +100, -125, -50, +385 kHz against the
     measured values above (same sign convention), and the V3c to +/-95 kHz. Replaying the whole tuner
     tune (`be7ff03`, rejected) changed nothing.
2. **V3c AUTO filter state.** Tuner readback after a cold boot: 0x0a/0x0b = `d5/6b` (the last writes
   of the reinit slice the V3c runs at start). The AUTO plan wrote `c5/8f`, the PC's pairing with
   its 1.815 MHz IF, together with the 3.570 MHz boot IF: a state neither the V3c boot nor any
   PC capture contains. After AUTO the left side stayed about 13 dB below boot.

## Differences between the PC and the previous driver (observed)

1. PC reads p0a r01 after every demod write; the driver did not (fixed).
2. PC programs the demod IF before the tuner tune; the driver after (not changed; centring is correct without it).
3. PC sends the full tuner tune each time; the driver skips cached values (not changed).
4. PC writes reg 0x10 = 0x84 at all three template positions; the driver writes 0xa4 at two.
5. PC 0x15 = c6 against the driver's c7 at 200 kHz (one SDM step).
6. PC turns the repeater off at the end; the bandwidth path does not (not changed).
7. PC AUTO pairs c5/8f with 1.815 MHz; the driver paired c5/8f with 3.570 MHz on the V3c (fixed differently: see below).
8. PC's retune after a bandwidth change writes no demod IF; the driver re-runs the bandwidth program.

## Fixes

- `fix(v3c,v4,v4l): read after each demod IF write in the live bandwidth path` — one helper,
  `measured_bandwidth_demod_if_records()`, used by every profile's bandwidth transaction.
- `fix(v3c): AUTO tuner bandwidth restores the boot filter state` — V3c AUTO = `d5/6b` + 3.570 MHz + `38 11 12`.
  Explicit 2.4 MHz keeps `c5/8f` + 1.815 MHz + `3b f7 78`. V4 and V4L AUTO plans unchanged.

The V3c runs a **variable IF by design**: 3.570 MHz at boot and AUTO, and the plan IF of the explicit
bandwidth (2.125, 2.025, 1.700, 1.750 or 1.815 MHz). The authoritative IF is the one in the
applied bandwidth plan; the PLL IF and the demod IF word are written from the same plan in one
paused transaction. The plan values are vendor-driver control choices derived from captures, not
measured analog passbands.

## Tab5 hardware acceptance (observed)

M5 Tab5 (ESP32-P4, ESP-IDF 5.5.4), FM 96.1 MHz, 2.4 MS/s, 20 s dwell per stage, 1 s raw IQ per
stage, no STEP. Carrier is a power-weighted centroid of the station (OrcSDR tool `gate_spectrum.py`).
Sequence: AUTO, 200k, 300k, 500k, 1.0M, 1.8M, 2.4M, AUTO. Every run: requested bandwidth = applied,
exactly one tuner tune per stage, no radio stop, zero rejected control records, zero USB overruns,
zero IQ drops, zero audio drops.

**V3c** (manual gain constant within each run from the baseline on; it settles during boot, before the gate begins, and differs between runs: 0 dB in A and D, 0.9 dB in B, C and E, 2.7 dB in F and G):

| run | driver code | boot | max carrier offset | final AUTO vs boot (12 offsets, +/-1.1 MHz) |
|---|---|---|---|---|
| A | fix + register readback (`6727710`) | cold | +1.6 kHz | within 0.6 dB |
| B | `dabba30` | cold | +1.1 kHz | within 0.6 dB |
| C | `dabba30` | cold (repeat) | +0.9 kHz | within 0.8 dB |
| D | `dabba30` | cold, then unplug/replug | +1.1 kHz | within 0.5 dB |
| E | `d363fc5` | cold | +1.0 kHz | within 0.7 dB |
| F | `07b418e` (shared helper) | cold | +1.9 kHz | up to **3.6 dB** (irregular, -0.3..-3.6; gain constant) |
| G | `07b418e` (repeat) | cold | +2.0 kHz (boot itself +2.1) | within 0.4 dB |

Run F's AUTO and the following +100/-100 kHz retune both sat 1-3.6 dB under the boot baseline with
the manual gain unchanged, and the repeat G did not reproduce it; the cause is not established (the
deviation is irregular across offsets, which fits broadcast-content drift, but that is a hypothesis).

Run A register readback taken after each stage's own IF write:

| stage | tuner 0a/0b | PLL IF | demod IF word |
|---|---|---|---|
| boot | d5/6b | 3.570 MHz | 38 11 12 |
| 200k, 300k | c5/e6 | 2.125 | 3b 47 1d |
| 500k | c5/e8 | 2.025 | 3b 80 00 |
| 1.0M | c5/eb | 1.700 | 3c 38 e4 |
| 1.8M | c5/ac | 1.750 | 3c 1c 72 |
| 2.4M | c5/8f | 1.815 | 3b f7 78 |
| AUTO | d5/6b | 3.570 | 38 11 12 |

**V4L** (driver `07b418e`; the V4L cold-boot run was preceded by a V3c session, i.e. the dongle was
hot-swapped in before the gate started), carrier offset in kHz, cold-boot run vs after unplug/replug,
and the same cold run before the fix:

| stage | before the fix | with the fix, cold | with the fix, replug |
|---|---|---|---|
| boot | -0.5 | +0.1 | -0.1 |
| 200k | -312.0 | -1.7 | -2.5 |
| 300k | -4.7 | -1.9 | -1.1 |
| 500k | +98.4 | +0.3 | +0.8 |
| 1.0M | -124.1 | +0.5 | +0.9 |
| 1.8M | -49.9 | +0.3 | +0.7 |
| 2.4M | +385.6 | +0.5 | +0.8 |
| AUTO | +0.3 | -0.9 | +1.0 |

Final AUTO within 0.3 dB (cold) and 0.5 dB (replug) of boot. The V4L narrows both sides symmetrically
at 200/300 kHz (about -11 to -13 dB at +/-900 kHz).

**V4** (driver `07b418e`; not measured before the fix): carrier +0.6 kHz at boot and +0.7 / +1.2 / +0.5 /
+0.4 / +0.9 / +0.2 / +0.1 kHz (cold) and +0.7 / +0.7 / +0.5 / +0.5 / +0.4 / +0.3 / -1.4 kHz (after
unplug/replug) through 200k, 300k, 500k, 1.0M, 1.8M, 2.4M and AUTO; final AUTO within 0.2 dB (cold) and
0.4 dB (replug) of boot. The V4 narrows the left side only (about -7 to -9 dB at -900 kHz at 200/300 kHz).

The left side of the V3c is cut by about 11-14 dB at 200/300 kHz (-900 kHz offset), 500 kHz less,
1.0 MHz less again, and not cut at 1.8/2.4 MHz. On the V3c, explicit bandwidth stages read about +2 to
+5 dB higher than boot on the right side in some runs; not investigated.

## Not established

- The internal RTL2832 mechanism of the read-after-write.
- Passbands in Hz (the plan values are control choices, not measurements).
- V4 behaviour before the fix (not run); the V4L result and the shared plan words suggest it shifted too.
- The cause of run F's level deviation.
- Low RDS/pilot values after a centred retune: seen in every V3c run, not investigated.
- Tuner registers 0x10..0x1f: a 32-byte tuner read failed on the bridge and only 0x00..0x0f were read back.
